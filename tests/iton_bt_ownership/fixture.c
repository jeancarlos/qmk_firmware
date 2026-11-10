// SPDX-License-Identifier: GPL-2.0-or-later
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "config.h"

void set_time(uint32_t time);

#define MCU_SN32
#define SPI_DATA_LENGTH(n) (n)
enum { SPI_READY = 2 };
typedef struct {
    struct { bool BUSY, RX_EMPTY, TX_EMPTY; } STAT_b;
    struct { bool RXFIFOTHIF; } RIS_b;
} SPIRegisters;
static SPIRegisters regs;
typedef struct {
    bool ready;
    unsigned state;
    SPIRegisters *spi;
} SPIDriver;
typedef struct {
    bool slave;
    void (*data_cb)(SPIDriver *);
    unsigned ctrl0;
} SPIConfig;
static SPIDriver SPID0 = {.spi = &regs};
static bool defer_fifo_reset;
#define SPI_FIFO_FRESET(spip) do { \
    (spip)->spi->STAT_b.TX_EMPTY = !defer_fifo_reset; \
} while (0)
#define A0 0
#define A1 1
static bool tx_active;
static bool rx_active;
static bool overlapping_start;
static bool stopped_tx;
static uint8_t *rx_buffer;
static const uint8_t *active_tx_buffer;
static bool final_byte_pending;
static bool drain_pending_on_stop;
static uint8_t pending_final_byte;
static size_t received_bytes;
static unsigned sends;
static size_t   sent_length;
static uint8_t  sent_bytes[16];
static bool     interrupt_high;
static bool     output_high;
static const SPIConfig *active_config;
static bool     gpio_read_pin(int pin) {
    return pin == A1 ? interrupt_high : pin == A0 && output_high;
}
static void gpio_write_pin_high(int pin) {
    if (pin == A0) output_high = true;
}
static void gpio_write_pin_low(int pin) {
    if (pin == A0) output_high = false;
}
static void gpio_set_pin_output(int pin) {
    (void)pin;
}
static void gpio_set_pin_input(int pin) {
    (void)pin;
}
static void spiStart(SPIDriver *driver, const SPIConfig *config) {
    driver->ready = true;
    driver->state = SPI_READY;
    active_config = config;
}
static void spiStartSend(SPIDriver *driver, size_t length, const void *data) {
    (void)driver;
    overlapping_start |= rx_active;
    tx_active = true;
    ++sends;
    sent_length = length;
    active_tx_buffer = data;
    memcpy(sent_bytes, data, length < sizeof(sent_bytes) ? length : sizeof(sent_bytes));
    if (!regs.STAT_b.TX_EMPTY && length <= sizeof(sent_bytes)) {
        memmove(&sent_bytes[1], sent_bytes, length - 1);
        sent_bytes[0] = 0; /* The master clocks the residual idle byte first. */
    }
}
static void chSysLockFromISR(void) {}
static void chSysUnlockFromISR(void) {}
static void chSysLock(void) {}
static void chSysUnlock(void) {}
static void spiStartSendI(SPIDriver *driver, size_t length, const void *data) {
    spiStartSend(driver, length, data);
}

static unsigned callback_registered;
static unsigned event_enabled;
#if PAL_USE_CALLBACKS
#    define PAL_EVENT_MODE_BOTH_EDGES 3
static void (*line_callback)(void *);
static void    *line_callback_arg;
static bool     trigger_edge;
static bool     receive_was_ready;
static unsigned receive_count;
static void     spiStartReceiveI(SPIDriver *driver, size_t len, void *data) {
    (void)len;
    overlapping_start |= tx_active;
    rx_active = true;
    rx_buffer = data;
    receive_was_ready = driver->ready;
    ++receive_count;
}
static void palSetLineCallback(int pin, void (*callback)(void *), void *arg) {
    (void)pin;
    line_callback     = callback;
    line_callback_arg = arg;
    ++callback_registered;
}
static void palEnableLineEvent(int pin, int mode) {
    (void)pin;
    (void)mode;
    ++event_enabled;
    if (trigger_edge && line_callback) {
        line_callback(line_callback_arg);
    }
}
#endif
static void spiStopTransferI(SPIDriver *driver, void *remaining) {
    (void)driver;
    if (!tx_active && !rx_active) return;
    if (final_byte_pending) {
        // Model the merged SN32 stop draining one already-completed FIFO byte.
        if (drain_pending_on_stop && rx_buffer) {
            rx_buffer[2] = pending_final_byte;
            ++received_bytes;
        }
        final_byte_pending = false;
    }
    if (remaining) *(size_t *)remaining = 16 - received_bytes;
    stopped_tx = tx_active;
    tx_active = false;
    rx_active = false;
}
#include "../../drivers/bluetooth/iton_bt.c"
unsigned test_queue_capacity(void) { return sizeof(iton_queue); }
void test_reset(void) {
    set_time(0);
    memset(&regs, 0, sizeof(regs));
    regs.STAT_b.TX_EMPTY = regs.STAT_b.RX_EMPTY = true;
    defer_fifo_reset = false;
    SPID0.ready         = false;
    tx_active = false;
    rx_active = false;
    overlapping_start = false;
    stopped_tx = false;
    final_byte_pending = false;
    drain_pending_on_stop = false;
    rx_buffer = NULL;
    active_tx_buffer = NULL;
    received_bytes = 0;
    sends = 0;
    sent_length = 0;
    memset(sent_bytes, 0, sizeof(sent_bytes));
    iton_bt_is_connected = false;
    iton_bt_led_state = 0;
    iton_owner = ITON_IDLE;
    iton_owner_since = 0;
    iton_tx_retry_len = 0;
    iton_retry_pending = 0;
    iton_ack_head = 0;
    iton_ack_count = 0;
    iton_last_was_ack = false;
    iton_head = 0;
    iton_count = 0;
    iton_pending_len = 0;
    iton_pending_keyboard = false;
    iton_consumer_press_pending = false;
    iton_consumer_release_pending = false;
    iton_keyboard_pending = false;
    iton_system_release_pending = false;
    iton_deferred_order = 0;
    iton_bt_queue_overflow = false;
    iton_bt_rx_collision = false;
    iton_bt_send_kb_last_key = 0;
    memset(iton_bt_rx_buffer, 0, sizeof(iton_bt_rx_buffer));
    memset(iton_bt_tx_buffer, 0, sizeof(iton_bt_tx_buffer));
    interrupt_high      = false;
    output_high = false;
    active_config = NULL;
    callback_registered = 0;
    event_enabled       = 0;
#if PAL_USE_CALLBACKS
    receive_count     = 0;
    trigger_edge      = false;
    line_callback     = NULL;
    line_callback_arg = NULL;
#endif
}
unsigned test_callback_registered(void) {
    return callback_registered;
}
unsigned test_event_enabled(void) {
    return event_enabled;
}
#if PAL_USE_CALLBACKS
void test_prepare_edge_on_enable(void) {
    interrupt_high    = true;
    trigger_edge      = true;
    receive_count     = 0;
    receive_was_ready = false;
}
unsigned test_receive_count(void) {
    return receive_count;
}
bool test_receiving(void) { return rx_active; }
void test_set_input_high(void) { interrupt_high = true; receive_count = 0; }
bool test_receive_was_ready(void) {
    return receive_was_ready;
}
#endif

void test_falling_edge(void) {
    interrupt_high = false;
    line_callback(line_callback_arg);
}
void test_complete_transfer(void) {
    tx_active = false;
    rx_active = false;
    active_config->data_cb(&SPID0);
}
bool test_output_high(void) { return output_high; }
bool test_overflow(void) { return iton_bt_queue_overflow; }
void test_send_sixth_key(uint8_t key) {
    report_keyboard_t report = {0};
    report.keys[5] = key;
    iton_bt_send_keyboard(&report);
}
void test_send_keyboard_pair(uint8_t first, uint8_t sixth) {
    report_keyboard_t report = {0};
    report.keys[0] = first;
    report.keys[5] = sixth;
    iton_bt_send_keyboard(&report);
}
uint8_t test_last_sixth_key(void) { return iton_bt_send_kb_last_key; }
bool test_rx_collision(void) { return iton_bt_rx_collision; }
unsigned test_sends(void) { return sends; }
uint8_t test_sent_byte(unsigned index) { return sent_bytes[index]; }
void test_tx_residue(void) { regs.STAT_b.TX_EMPTY = false; }
void test_spi_busy(bool busy) { regs.STAT_b.BUSY = busy; }
void test_spi_irq_pending(bool pending) { regs.RIS_b.RXFIFOTHIF = pending; }
void test_defer_fifo_reset(void) { defer_fifo_reset = true; }
void test_complete_fifo_reset(void) {
    defer_fifo_reset = false;
    regs.STAT_b.TX_EMPTY = true;
}
void test_full_receive(void) {
    interrupt_high = true;
    line_callback(line_callback_arg);
    rx_buffer[0] = 0xb6;
    rx_buffer[1] = 0x51;
    rx_buffer[2] = 0x76;
    received_bytes = 16;
    test_complete_transfer();
    test_falling_edge();
}
void test_rising_edge(void) {
    interrupt_high = true;
    line_callback(line_callback_arg);
}
bool test_overlapping_start(void) { return overlapping_start; }
bool test_stopped_tx(void) { return stopped_tx; }
bool test_sent_release(void) {
    return sends == 1 && sent_length == 3 && sent_bytes[0] == 0xa3 && sent_bytes[1] == 0 && sent_bytes[2] == 0;
}
bool test_receive_preserves_active_send(void) {
    uint8_t payload = 0x42;
    iton_bt_send(0xa3, &payload, 1);
    test_rising_edge();
    if (rx_buffer) {
        rx_buffer[0] = 0xb6;
        rx_buffer[1] = 0x51;
    }
    return active_tx_buffer[0] == 0xa3 && active_tx_buffer[1] == 0x42;
}

void test_receive_notification(bool pending) {
    interrupt_high = true;
    line_callback(line_callback_arg);
    rx_buffer[0] = 0xb6;
    rx_buffer[1] = 0x51;
    received_bytes = pending ? 2 : 3;
    if (pending) {
        pending_final_byte = 0x76;
        final_byte_pending = true;
    } else {
        rx_buffer[2] = 0x76;
    }
    interrupt_high = false;
    line_callback(line_callback_arg);
}
void test_oversized_notification(void) {
    test_rising_edge();
    rx_buffer[0] = 0xb6;
    rx_buffer[1] = 0x51;
    rx_buffer[2] = 0x76;
    rx_buffer[3] = 0xff;
    received_bytes = 4;
    test_falling_edge();
}
void test_short_notification(void) {
    test_receive_notification(false);
    iton_bt_is_connected = false;
    test_rising_edge();
    rx_buffer[0] = 0xb6;
    rx_buffer[1] = 0x51;
    received_bytes = 2;
    test_falling_edge();
}
void test_receive_ack_notification(uint8_t type, uint8_t value) {
    test_rising_edge();
    rx_buffer[0] = 0xb6;
    rx_buffer[1] = type;
    rx_buffer[2] = value;
    received_bytes = 3;
    test_falling_edge();
}
bool test_connected(void) { return iton_bt_is_connected; }
void test_enable_pending_drain(void) { drain_pending_on_stop = true; }

void test_receive_led(uint8_t value) {
    test_rising_edge();
    rx_buffer[0] = 0xb1;
    rx_buffer[1] = value;
    received_bytes = 2;
    test_falling_edge();
}
