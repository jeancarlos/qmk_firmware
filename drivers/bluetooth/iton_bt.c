// Copyright 2024 1Conan (@1Conan) <me@1conan.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string.h>
#include <hal.h>
#include "gpio.h"
#include "config.h"
#include "iton_bt.h"
#include "timer.h"
#ifdef ITON_BT_TIMING
#    include "print.h"
#endif

#ifndef ITON_BT_SPI_TIMEOUT_MS
#    define ITON_BT_SPI_TIMEOUT_MS 100
#endif

#ifndef ITON_BT_SPI_PORT
#    define ITON_BT_SPI_PORT SPID0
#endif

#ifndef ITON_BT_IRQ_LINE
#    define ITON_BT_IRQ_LINE A0
#endif

#ifndef ITON_BT_INT_LINE
#    define ITON_BT_INT_LINE A1
#endif

#ifndef ITON_BT_BUFFER_LEN
#    define ITON_BT_BUFFER_LEN 16
#endif

// The NKRO frame contains a command byte and a 15-byte report.
#if ITON_BT_BUFFER_LEN < 16
#    error "ITON_BT_BUFFER_LEN must be at least 16 bytes"
#endif

#ifndef ITON_BT_QUEUE_SIZE
#    define ITON_BT_QUEUE_SIZE 32
#endif

#if ITON_BT_QUEUE_SIZE < 27 || ITON_BT_QUEUE_SIZE > 255
#    error "ITON_BT_QUEUE_SIZE must fit a paired keyboard report and an 8-bit count"
#endif

/**
 * Driver Macros
 */
#define HIGH_BITS(x) ((uint8_t)(x >> 8))
#define LOW_BITS(x) ((uint8_t)(x & 0x00FF))

/**
 * Function definitions
 */
void iton_bt_data_cb(SPIDriver *spip);

/**
 * Callbacks
 */
__attribute__((weak)) void iton_bt_battery_voltage_low(void) {}
__attribute__((weak)) void iton_bt_battery_exit_low_battery_mode(void) {}
__attribute__((weak)) void iton_bt_battery_low_power_shutdown(void) {}
__attribute__((weak)) void iton_bt_battery_level(uint8_t level) {}

__attribute__((weak)) void iton_bt_connection_successful(void) {}
__attribute__((weak)) void iton_bt_entered_pairing(void) {}
__attribute__((weak)) void iton_bt_disconnected(void) {}
__attribute__((weak)) void iton_bt_enters_connection_state(void) {}
__attribute__((weak)) void iton_bt_profile_sent(uint8_t profile) {}

/**
 * Driver variables
 */
bool iton_bt_is_connected = false;
#ifdef ITON_BT_ENABLE_LED_STATE
uint8_t iton_bt_led_state = 0x00;
#endif

#if defined(PAL_USE_CALLBACKS) && PAL_USE_CALLBACKS
static uint8_t iton_bt_rx_buffer[ITON_BT_BUFFER_LEN];
#endif
static uint8_t iton_bt_tx_buffer[ITON_BT_BUFFER_LEN];
enum iton_bt_owner { ITON_IDLE, ITON_TX, ITON_RX, ITON_RX_FULL, ITON_FIFO_RESET, ITON_TX_RECOVER, ITON_RX_DISCARD };
static uint8_t iton_owner;
static uint32_t iton_owner_since;
static uint8_t iton_tx_len, iton_tx_retry_len, iton_retry_pending;
static uint8_t iton_ack_queue[4][2];
static uint8_t iton_ack_head, iton_ack_count;
static bool iton_last_was_ack;
static uint8_t iton_queue[ITON_BT_QUEUE_SIZE];
static uint8_t iton_head, iton_count;
#ifdef ITON_BT_TIMING
static struct {
    uint16_t tx, completed, tx_timeout, rx_timeout, fifo_timeout, retries, ack_dropped, deferred;
    uint16_t first_byte_ms, complete_ms, backlog_ms;
    uint8_t queue_peak;
} iton_timing;
static uint32_t iton_backlog_since;
static bool iton_backlogged, iton_first_byte_seen;

#    define ITON_TIMING_INC(field) (++iton_timing.field)

static void iton_timing_max(uint16_t *max, uint32_t value) {
    if (value > *max) *max = value > UINT16_MAX ? UINT16_MAX : value;
}

/* Foreground sampling gives an upper bound for the first completed byte. */
static void iton_bt_measure_locked(uint32_t now) {
    if (iton_owner == ITON_TX && !iton_first_byte_seen) {
#    if defined(MCU_SN32)
        if (ITON_BT_SPI_PORT.idx != 0) {
            iton_timing_max(&iton_timing.first_byte_ms, now - iton_owner_since);
            iton_first_byte_seen = true;
        }
#    endif
    }
    if (iton_count > iton_timing.queue_peak) iton_timing.queue_peak = iton_count;
    if (iton_backlogged) iton_timing_max(&iton_timing.backlog_ms, now - iton_backlog_since);
    if (iton_count && !iton_backlogged) iton_backlog_since = now;
    iton_backlogged = iton_count != 0;
}

void iton_bt_log_timing(void) {
    chSysLock();
    typeof(iton_timing) snapshot = iton_timing;
    chSysUnlock();
    uprintf("iton tx=%u done=%u retry=%u timeout tx/rx/fifo=%u/%u/%u\n",
            snapshot.tx, snapshot.completed, snapshot.retries, snapshot.tx_timeout, snapshot.rx_timeout, snapshot.fifo_timeout);
    uprintf("iton peak=%u deferred=%u ackdrop=%u max ms first/done/backlog=%u/%u/%u\n",
            snapshot.queue_peak, snapshot.deferred, snapshot.ack_dropped, snapshot.first_byte_ms, snapshot.complete_ms, snapshot.backlog_ms);
}
#else
#    define ITON_TIMING_INC(field) ((void)0)
#endif
static uint8_t iton_pending[ITON_BT_BUFFER_LEN];
static uint8_t iton_pending_len;
static bool iton_pending_keyboard;
static bool iton_consumer_press_pending;
static uint16_t iton_consumer_press;
static bool iton_consumer_release_pending;
static uint8_t iton_keyboard_latest[8];
static bool iton_keyboard_pending;
static bool iton_system_release_pending;
static uint16_t iton_deferred_order;
enum { ITON_DEFER_KEYBOARD = 1, ITON_DEFER_CONSUMER = 2, ITON_DEFER_SYSTEM = 3, ITON_DEFER_CONSUMER_PRESS = 4 };

/* Called before setting the corresponding pending flag, under the lock. */
static void iton_defer(uint8_t type) {
    uint8_t count = iton_keyboard_pending + iton_consumer_release_pending + iton_system_release_pending + iton_consumer_press_pending;
    iton_deferred_order |= (uint16_t)type << (count * 3);
}
bool iton_bt_queue_overflow;
bool iton_bt_rx_collision;
uint8_t        iton_bt_send_kb_last_key = 0x00;

/* Called with the system locked, from either thread or interrupt context. */
static void iton_enqueue_direct(uint8_t cmd, const uint8_t *data, uint8_t len) {
    uint8_t slot = (iton_head + iton_count) % ITON_BT_QUEUE_SIZE;
    iton_queue[slot] = len + 1;
    slot = (slot + 1) % ITON_BT_QUEUE_SIZE;
    iton_queue[slot] = cmd;
    for (uint8_t i = 0; i < len; ++i) {
        slot = (slot + 1) % ITON_BT_QUEUE_SIZE;
        iton_queue[slot] = data[i];
    }
    iton_count += len + 2;
}

static bool iton_enqueue(uint8_t cmd, const uint8_t *data, uint8_t len) {
    if (iton_pending_len || iton_consumer_press_pending || iton_consumer_release_pending || iton_keyboard_pending || iton_system_release_pending) {
        iton_bt_queue_overflow = true;
        return false;
    }
    if (len + 2 > ITON_BT_QUEUE_SIZE - iton_count) {
        iton_pending[0] = cmd;
        memcpy(&iton_pending[1], data, len);
        iton_pending_len = len + 1;
        return true;
    }
    iton_enqueue_direct(cmd, data, len);
    return true;
}

/* Called locked; the latest report may replace intermediate rejected states. */
static bool iton_enqueue_keyboard(const uint8_t *report) {
    uint8_t key = (report[7] >> 3) < 15 ? report[7] : 0;
    bool changed = key != iton_bt_send_kb_last_key;
    uint8_t needed = changed ? 27 : 10;
    if (ITON_BT_QUEUE_SIZE - iton_count < needed) return false;
    iton_enqueue_direct(report_hid, report, 8);
    if (changed) {
        uint8_t bitmap[15] = {0};
        if (key) bitmap[key >> 3] = 1u << (key & 7);
        iton_enqueue_direct(report_nkro, bitmap, sizeof(bitmap));
        iton_bt_send_kb_last_key = key;
    }
    return true;
}

const SPIConfig iton_bt_spicfg = {
    .slave   = true,
    .data_cb = iton_bt_data_cb,
#if defined(MCU_SN32)
    .ctrl0 = SPI_DATA_LENGTH(8),
#endif
};

/**
 * Callbacks
 */
#if defined(PAL_USE_CALLBACKS) && PAL_USE_CALLBACKS
#    ifndef ITON_BT_UNKNOWN_ACK
#        define ITON_BT_UNKNOWN_ACK unknown_ack
#    endif
static inline void iton_bt_rx_battery_notif(uint8_t data) {
    switch (data) {
        case batt_voltage_low:
            iton_bt_battery_voltage_low();
            break;
        case batt_exit_low_battery_mode:
            iton_bt_battery_exit_low_battery_mode();
            break;
        case batt_low_power_shutdown:
            iton_bt_battery_low_power_shutdown();
            break;
        case batt_above_70:
        case batt_between_30_70:
        case batt_below_30:
            iton_bt_battery_level(data);
            break;
        case batt_wake_mcu:
#    ifdef ITON_BT_ENABLE_ACK
            iton_bt_send_ack(control_bt, wake_ack);
#    endif
            break;
        case batt_unknown:
#    ifdef ITON_BT_ENABLE_ACK
            iton_bt_send_ack(control_bt, ITON_BT_UNKNOWN_ACK);
#    endif
            break;
        case query_working_mode:
            break;
        case query_bt_name:
            break;
    }
}

static inline void iton_bt_rx_bluetooth_notif(uint8_t data) {
    switch (data) {
        case bt_connection_success:
            iton_bt_is_connected = true;

#    ifdef ITON_BT_ENABLE_ACK
            iton_bt_send_ack(control_bt, connect_ack);
#    endif

            iton_bt_connection_successful();
            break;
        case bt_entered_pairing:
            iton_bt_entered_pairing();
            break;
        case bt_disconected:
            iton_bt_is_connected = false;

#    ifdef ITON_BT_ENABLE_ACK
            iton_bt_send_ack(control_bt, disconnect_ack);
#    endif

            iton_bt_disconnected();
            break;
        case bt_enters_connection:
            iton_bt_enters_connection_state();
            break;
    }
}

static void iton_bt_start_receive_locked(void) {
    iton_owner = ITON_RX;
    iton_owner_since = timer_read32();
    spiStartReceiveI(&ITON_BT_SPI_PORT, sizeof(iton_bt_rx_buffer), iton_bt_rx_buffer);
}

static void iton_bt_rx_cb(void *arg) {
    (void)arg;
    if (gpio_read_pin(ITON_BT_INT_LINE)) {
        chSysLockFromISR();
        if (iton_owner == ITON_IDLE) {
            iton_bt_start_receive_locked();
        } else if (iton_owner == ITON_TX) {
            iton_bt_rx_collision = true;
        }
        chSysUnlockFromISR();
    } else {
        size_t remaining = ITON_BT_BUFFER_LEN;
        uint8_t frame[3] = {0};
        chSysLockFromISR();
        if (iton_owner == ITON_RX_DISCARD) iton_owner = ITON_IDLE;
        if (iton_owner != ITON_RX && iton_owner != ITON_RX_FULL) {
            chSysUnlockFromISR();
            return;
        }
        if (iton_owner == ITON_RX) {
            spiStopTransferI(&ITON_BT_SPI_PORT, &remaining);
        }
        memcpy(frame, iton_bt_rx_buffer, sizeof(frame));
        iton_owner = ITON_IDLE;
        chSysUnlockFromISR();

        size_t received = ITON_BT_BUFFER_LEN - remaining;
        if (received != 2 && received != 3) {
            return;
        }

        switch (frame[0]) {
            case led_state:
#    ifdef ITON_BT_ENABLE_LED_STATE
                if (received == 2) {
                    iton_bt_led_state = frame[1];
                }
#    endif
                break;
            case notification:
                if (received != 3) {
                    break;
                }
                switch (frame[1]) {
                    case notif_battery:
                        iton_bt_rx_battery_notif(frame[2]);
                        break;
                    case notif_bluetooth:
                        iton_bt_rx_bluetooth_notif(frame[2]);
                        break;
                }
                break;
        }
    }
}
#endif

void iton_bt_data_cb(SPIDriver *spip) {
    (void)spip;
    uint8_t profile = UINT8_MAX;
    chSysLockFromISR();
    if (iton_owner == ITON_TX) {
        if (iton_tx_len == 3 && iton_bt_tx_buffer[0] == control && iton_bt_tx_buffer[1] == control_bt &&
            iton_bt_tx_buffer[2] >= switch_profile && iton_bt_tx_buffer[2] < switch_profile + 6) {
            profile = iton_bt_tx_buffer[2] - switch_profile;
        }
        ITON_TIMING_INC(completed);
#ifdef ITON_BT_TIMING
        uint32_t elapsed = timer_read32() - iton_owner_since;
        if (!iton_first_byte_seen) iton_timing_max(&iton_timing.first_byte_ms, elapsed);
        iton_timing_max(&iton_timing.complete_ms, elapsed);
#endif
        iton_owner = ITON_IDLE;
#if defined(PAL_USE_CALLBACKS) && PAL_USE_CALLBACKS
        // ChibiOS marks SPI ready before the completion callback. Arm RX
        // before releasing the TX handshake if the peer is still requesting it.
        if (gpio_read_pin(ITON_BT_INT_LINE)) {
            iton_bt_rx_collision = true;
            iton_bt_start_receive_locked();
        }
#endif
        gpio_write_pin_low(ITON_BT_IRQ_LINE);
    } else if (iton_owner == ITON_RX) {
        iton_owner = ITON_RX_FULL;
    }
    chSysUnlockFromISR();
    if (profile != UINT8_MAX) iton_bt_profile_sent(profile);
}

/**
 * Driver Functions
 */
void iton_bt_init(void) {
    gpio_set_pin_output(ITON_BT_IRQ_LINE);
    gpio_set_pin_input(ITON_BT_INT_LINE);

    gpio_write_pin_low(ITON_BT_IRQ_LINE);

    spiStart(&ITON_BT_SPI_PORT, &iton_bt_spicfg);

#if defined(PAL_USE_CALLBACKS) && PAL_USE_CALLBACKS
    palSetLineCallback(ITON_BT_INT_LINE, iton_bt_rx_cb, NULL);
    palEnableLineEvent(ITON_BT_INT_LINE, PAL_EVENT_MODE_BOTH_EDGES);
    chSysLock();
    if (iton_owner == ITON_IDLE && gpio_read_pin(ITON_BT_INT_LINE)) {
        iton_bt_start_receive_locked();
    }
    chSysUnlock();
#endif
}

#if defined(MCU_SN32)
static bool iton_bt_prepare_send_locked(void) {
    SPIDriver *spip = &ITON_BT_SPI_PORT;
    if (spip->state != SPI_READY || gpio_read_pin(ITON_BT_IRQ_LINE) || !spip->spi->STAT_b.RX_EMPTY || spip->spi->RIS_b.RXFIFOTHIF) {
        return false;
    }

    // The SN32 ISR can leave a zero for tail clocks. BUSY can stay asserted
    // after the handshake ends, even when that zero is the only byte left.
    if (!spip->spi->STAT_b.TX_EMPTY) {
        if (gpio_read_pin(ITON_BT_INT_LINE)) return false;
        iton_owner = ITON_FIFO_RESET;
        iton_owner_since = timer_read32();
        SPI_FIFO_FRESET(spip);
        // FRESET is write-only. TX_EMPTY reports when the old byte is gone.
        if (!spip->spi->STAT_b.TX_EMPTY) {
            return false;
        }
        iton_owner = ITON_IDLE;
        if (!spip->spi->STAT_b.RX_EMPTY || spip->spi->RIS_b.RXFIFOTHIF) {
            return false;
        }
    }

    return !gpio_read_pin(ITON_BT_INT_LINE);
}
#endif

/* Called locked. Leave IRQ low for at least one timer tick after an abort. */
static void iton_bt_recover_locked(void) {
    uint32_t now = timer_read32();
    uint32_t age = now - iton_owner_since;
    bool input_high = gpio_read_pin(ITON_BT_INT_LINE);

    switch (iton_owner) {
        case ITON_TX:
            if (age < ITON_BT_SPI_TIMEOUT_MS) break;
            ITON_TIMING_INC(tx_timeout);
            gpio_write_pin_low(ITON_BT_IRQ_LINE);
            spiStopTransferI(&ITON_BT_SPI_PORT, NULL);
            iton_retry_pending = iton_tx_retry_len;
            iton_owner_since = now;
            iton_owner = ITON_TX_RECOVER;
            break;
        case ITON_TX_RECOVER:
            if (age >= 2) iton_owner = ITON_IDLE;
            break;
        case ITON_RX:
        case ITON_RX_FULL:
            if (age < ITON_BT_SPI_TIMEOUT_MS) break;
            ITON_TIMING_INC(rx_timeout);
            if (iton_owner == ITON_RX) spiStopTransferI(&ITON_BT_SPI_PORT, NULL);
            // Ignore the rest of the truncated frame until the peer ends it.
            iton_owner = input_high ? ITON_RX_DISCARD : ITON_IDLE;
            break;
        case ITON_RX_DISCARD:
            if (!input_high) iton_owner = ITON_IDLE;
            break;
#if defined(MCU_SN32)
        case ITON_FIFO_RESET: {
            SPIDriver *spip = &ITON_BT_SPI_PORT;
            if (spip->spi->STAT_b.TX_EMPTY) {
                iton_owner = ITON_IDLE;
            } else if (!input_high && age >= ITON_BT_SPI_TIMEOUT_MS) {
                ITON_TIMING_INC(fifo_timeout);
                SPI_FIFO_FRESET(spip);
                iton_owner_since = now;
            }
            break;
        }
#endif
        default:
            break;
    }
}

/* Pop a regular report, then use the freed space for deferred states. */
static uint8_t iton_bt_dequeue_report_locked(void) {
    uint8_t len = iton_queue[iton_head];
    iton_head = (iton_head + 1) % ITON_BT_QUEUE_SIZE;
    for (uint8_t i = 0; i < len; ++i) {
        iton_bt_tx_buffer[i] = iton_queue[iton_head];
        iton_head = (iton_head + 1) % ITON_BT_QUEUE_SIZE;
    }
    iton_count -= len + 1;
    if (iton_pending_len) {
        bool queued = false;
        if (iton_pending_keyboard) {
            queued = iton_enqueue_keyboard(iton_pending);
        } else if (iton_pending_len + 1 <= ITON_BT_QUEUE_SIZE - iton_count) {
            iton_enqueue_direct(iton_pending[0], &iton_pending[1], iton_pending_len - 1);
            queued = true;
        }
        if (queued) {
            iton_pending_len = 0;
            iton_pending_keyboard = false;
        }
    }
    // Deferred states are ordered by first rejection.
    if (!iton_pending_len && iton_deferred_order) {
        uint8_t kind = iton_deferred_order & 7;
        bool queued = false;
        if (kind == ITON_DEFER_KEYBOARD && iton_enqueue_keyboard(iton_keyboard_latest)) {
            iton_keyboard_pending = false;
            queued = true;
        } else if (kind == ITON_DEFER_CONSUMER_PRESS && ITON_BT_QUEUE_SIZE - iton_count >= 4) {
            const uint8_t press[2] = {HIGH_BITS(iton_consumer_press), LOW_BITS(iton_consumer_press)};
            iton_enqueue_direct(report_consumer, press, sizeof(press));
            iton_consumer_press_pending = false;
            queued = true;
        } else if (kind == ITON_DEFER_CONSUMER && ITON_BT_QUEUE_SIZE - iton_count >= 4) {
            const uint8_t release[2] = {0, 0};
            iton_enqueue_direct(report_consumer, release, sizeof(release));
            iton_consumer_release_pending = false;
            queued = true;
        } else if (kind == ITON_DEFER_SYSTEM && ITON_BT_QUEUE_SIZE - iton_count >= 3) {
            const uint8_t release = 0;
            iton_enqueue_direct(report_system, &release, 1);
            iton_system_release_pending = false;
            queued = true;
        }
        if (queued) iton_deferred_order >>= 3;
    }
    return len;
}

void iton_bt_task(void) {
    chSysLock();
#ifdef ITON_BT_TIMING
    iton_bt_measure_locked(timer_read32());
#endif
    if (iton_owner != ITON_IDLE) iton_bt_recover_locked();
#if defined(PAL_USE_CALLBACKS) && PAL_USE_CALLBACKS
    // INT may rise during recovery while the edge callback cannot arm RX.
    if (iton_owner == ITON_IDLE && gpio_read_pin(ITON_BT_INT_LINE)) {
        iton_bt_start_receive_locked();
    }
#endif
    if (iton_owner != ITON_IDLE || (!iton_count && !iton_ack_count && !iton_retry_pending) || gpio_read_pin(ITON_BT_INT_LINE)) {
        chSysUnlock();
        return;
    }
#if defined(MCU_SN32)
    if (!iton_bt_prepare_send_locked()) {
        chSysUnlock();
        return;
    }
#endif

    uint8_t len;
    if (iton_retry_pending) {
        ITON_TIMING_INC(retries);
        len = iton_retry_pending;
        iton_retry_pending = 0;
        iton_tx_retry_len = 0;
    } else if (iton_ack_count && (!iton_count || !iton_last_was_ack)) {
        iton_bt_tx_buffer[0] = control;
        memcpy(&iton_bt_tx_buffer[1], iton_ack_queue[iton_ack_head], 2);
        iton_ack_head = (iton_ack_head + 1) % (sizeof(iton_ack_queue) / sizeof(iton_ack_queue[0]));
        --iton_ack_count;
        len = 3;
        iton_tx_retry_len = 0;
        iton_last_was_ack = true;
    } else {
        len = iton_bt_dequeue_report_locked();
        uint8_t cmd = iton_bt_tx_buffer[0];
        // Retry absolute key states and releases once, without replaying mode commands.
        bool release = (cmd == report_consumer && len == 3 && !iton_bt_tx_buffer[1] && !iton_bt_tx_buffer[2]) ||
                       (cmd == report_system && len == 2 && !iton_bt_tx_buffer[1]);
        iton_tx_retry_len = cmd == report_hid || cmd == report_nkro || release ? len : 0;
        iton_last_was_ack = false;
    }
    iton_owner_since = timer_read32();
    iton_owner = ITON_TX;
    ITON_TIMING_INC(tx);
#ifdef ITON_BT_TIMING
    iton_first_byte_seen = false;
#endif
    iton_tx_len = len;
    spiStartSendI(&ITON_BT_SPI_PORT, len, iton_bt_tx_buffer);
    gpio_write_pin_high(ITON_BT_IRQ_LINE);
    chSysUnlock();
}

bool iton_bt_is_idle(void) {
    chSysLock();
    bool idle = iton_owner == ITON_IDLE && !iton_ack_count && !iton_retry_pending && iton_count == 0 && iton_pending_len == 0 &&
                !iton_consumer_press_pending && !iton_consumer_release_pending && !iton_keyboard_pending && !iton_system_release_pending && !gpio_read_pin(ITON_BT_INT_LINE);
    chSysUnlock();
    return idle;
}

bool iton_bt_enter_pairing_if(bool (*ready)(void)) {
    const uint8_t data[] = {control_bt, enter_pairing};
    chSysLock();
    bool queued = !gpio_read_pin(ITON_BT_INT_LINE) && ready() && iton_enqueue(control, data, sizeof(data));
    chSysUnlock();
    if (queued) iton_bt_task();
    return queued;
}

static bool iton_bt_try_send(uint8_t cmd, uint8_t *data, uint8_t len) {
    if (len > sizeof(iton_bt_tx_buffer) - 1) return false;
    chSysLock();
    bool queued = iton_enqueue(cmd, data, len);
    chSysUnlock();
    iton_bt_task();
    return queued;
}

void iton_bt_send(uint8_t cmd, uint8_t *data, uint8_t len) {
    (void)iton_bt_try_send(cmd, data, len);
}

bool iton_bt_send2(uint8_t cmd, uint8_t b1, uint8_t b2) {
    uint8_t data[] = {b1, b2};
    return iton_bt_try_send(cmd, data, sizeof(data));
}

void iton_bt_send_ack(uint8_t b1, uint8_t b2) {
    chSysLockFromISR();
    const uint8_t capacity = sizeof(iton_ack_queue) / sizeof(iton_ack_queue[0]);
    if (iton_ack_count < capacity) {
        uint8_t slot = (iton_ack_head + iton_ack_count) % capacity;
        iton_ack_queue[slot][0] = b1;
        iton_ack_queue[slot][1] = b2;
        ++iton_ack_count;
    } else {
        ITON_TIMING_INC(ack_dropped);
    }
    chSysUnlockFromISR();
}

void iton_bt_send_fn(bool pressed) {
    uint8_t data = pressed ? 0xA3 : 0x00;

    iton_bt_send(report_fn, &data, 1);
}

void iton_bt_send_system(uint16_t data) {
    uint8_t payload = LOW_BITS(data);
    chSysLock();
    bool queued = iton_enqueue(report_system, &payload, 1);
    if (!queued && data == 0 && !iton_system_release_pending) {
        iton_defer(ITON_DEFER_SYSTEM);
        iton_system_release_pending = true;
    }
    chSysUnlock();
    iton_bt_task();
}

void iton_bt_send_consumer(uint16_t data) {
    uint8_t payload[] = {HIGH_BITS(data), LOW_BITS(data)};
    chSysLock();
    bool queued = iton_enqueue(report_consumer, payload, sizeof(payload));
    if (!queued && data != 0 && !iton_consumer_press_pending && !iton_consumer_release_pending) {
        iton_defer(ITON_DEFER_CONSUMER_PRESS);
        iton_consumer_press = data;
        iton_consumer_press_pending = true;
    } else if (!queued && data == 0 && !iton_consumer_release_pending) {
        iton_defer(ITON_DEFER_CONSUMER);
        iton_consumer_release_pending = true;
    }
    chSysUnlock();
    iton_bt_task();
}

void iton_bt_send_keyboard(report_keyboard_t *report) {
    chSysLock();
    if (iton_pending_len || iton_consumer_press_pending || iton_consumer_release_pending || iton_keyboard_pending || iton_system_release_pending ||
        !iton_enqueue_keyboard(&report->mods)) {
        ITON_TIMING_INC(deferred);
        if (!iton_pending_len && !iton_consumer_press_pending && !iton_consumer_release_pending && !iton_keyboard_pending && !iton_system_release_pending) {
            memcpy(iton_pending, &report->mods, sizeof(iton_keyboard_latest));
            iton_pending_len = sizeof(iton_keyboard_latest);
            iton_pending_keyboard = true;
        } else {
            if (!iton_keyboard_pending) iton_defer(ITON_DEFER_KEYBOARD);
            memcpy(iton_keyboard_latest, &report->mods, sizeof(iton_keyboard_latest));
            iton_keyboard_pending = true;
        }
        iton_bt_queue_overflow = true;
    }
    chSysUnlock();
    iton_bt_task();
}
