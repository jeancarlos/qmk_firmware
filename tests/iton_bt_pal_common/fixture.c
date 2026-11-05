// SPDX-License-Identifier: GPL-2.0-or-later
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "config.h"

typedef struct {
    bool ready;
} SPIDriver;
typedef struct {
    bool slave;
    void (*data_cb)(SPIDriver *);
} SPIConfig;
static SPIDriver SPID0;
#define A0 0
#define A1 1
static unsigned sends;
static size_t   sent_length;
static uint8_t  sent_bytes[16];
static bool     interrupt_high;
static bool     gpio_read_pin(int pin) {
    return pin == A1 && interrupt_high;
}
static void gpio_write_pin_high(int pin) {
    (void)pin;
}
static void gpio_write_pin_low(int pin) {
    (void)pin;
}
static void gpio_set_pin_output(int pin) {
    (void)pin;
}
static void gpio_set_pin_input(int pin) {
    (void)pin;
}
static void spiStart(SPIDriver *driver, const SPIConfig *config) {
    driver->ready = true;
    (void)config;
}
static void spiStartSend(SPIDriver *driver, size_t length, const void *data) {
    (void)driver;
    ++sends;
    sent_length = length;
    memcpy(sent_bytes, data, length < sizeof(sent_bytes) ? length : sizeof(sent_bytes));
}
static void chSysLockFromISR(void) {}
static void chSysUnlockFromISR(void) {}
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
    (void)data;
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
#define spiStopTransferI(driver, remaining) ((void)0)
#include "../../drivers/bluetooth/iton_bt.c"
void test_reset(void) {
    SPID0.ready         = false;
    interrupt_high      = false;
    callback_registered = 0;
    event_enabled       = 0;
#if PAL_USE_CALLBACKS
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
bool test_receive_was_ready(void) {
    return receive_was_ready;
}
#endif
