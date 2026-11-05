// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
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
#define PAL_EVENT_MODE_BOTH_EDGES 3

static bool           interrupt_high;
static uint8_t       *rx_buffer;
static const uint8_t *tx_buffer;
static void (*line_callback)(void *);
static void *line_callback_arg;

static bool gpio_read_pin(int pin) {
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
static void chSysLockFromISR(void) {}
static void chSysUnlockFromISR(void) {}
static void spiStart(SPIDriver *driver, const SPIConfig *config) {
    driver->ready = true;
    (void)config;
}
static void spiStartSend(SPIDriver *driver, size_t length, const void *data) {
    (void)driver;
    (void)length;
    tx_buffer = data;
}
static void spiStartSendI(SPIDriver *driver, size_t length, const void *data) {
    spiStartSend(driver, length, data);
}
static void spiStartReceiveI(SPIDriver *driver, size_t length, void *data) {
    (void)driver;
    (void)length;
    rx_buffer = data;
}
static void spiStopTransferI(SPIDriver *driver, void *remaining) {
    (void)driver;
    (void)remaining;
}
static void palSetLineCallback(int pin, void (*callback)(void *), void *arg) {
    (void)pin;
    line_callback     = callback;
    line_callback_arg = arg;
}
static void palEnableLineEvent(int pin, int mode) {
    (void)pin;
    (void)mode;
}

#include "../../drivers/bluetooth/iton_bt.c"

void test_reset(void) {
    interrupt_high    = false;
    rx_buffer         = NULL;
    tx_buffer         = NULL;
    line_callback     = NULL;
    line_callback_arg = NULL;
}
void test_begin_receive(void) {
    interrupt_high = true;
    line_callback(line_callback_arg);
    rx_buffer[0] = 0xb6;
    rx_buffer[1] = 0x51;
    rx_buffer[2] = 0x76;
}
bool test_receive_buffer_is_unchanged(void) {
    return rx_buffer[0] == 0xb6 && rx_buffer[1] == 0x51 && rx_buffer[2] == 0x76;
}
bool test_send_buffer_is_unchanged(void) {
    return tx_buffer[0] == 0xa3 && tx_buffer[1] == 0x12 && tx_buffer[2] == 0x34;
}
