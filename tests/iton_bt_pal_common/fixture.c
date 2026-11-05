// SPDX-License-Identifier: GPL-2.0-or-later
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "config.h"

typedef struct {
    int unused;
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
static bool     gpio_read_pin(int pin) {
    (void)pin;
    return false;
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
    (void)driver;
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
static void palSetLineCallback(int pin, void (*callback)(void *), void *arg) {
    (void)pin;
    (void)callback;
    (void)arg;
    ++callback_registered;
}
static void palEnableLineEvent(int pin, int mode) {
    (void)pin;
    (void)mode;
    ++event_enabled;
}
#endif
#define spiStartReceiveI(driver, len, data) ((void)0)
#define spiStopTransferI(driver, remaining) ((void)0)
#include "../../drivers/bluetooth/iton_bt.c"
unsigned test_callback_registered(void) {
    return callback_registered;
}
unsigned test_event_enabled(void) {
    return event_enabled;
}
