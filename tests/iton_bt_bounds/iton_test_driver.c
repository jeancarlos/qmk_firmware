// SPDX-License-Identifier: GPL-2.0-or-later
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

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
static void chSysLock(void) {}
static void chSysUnlock(void) {}
static void spiStartSendI(SPIDriver *driver, size_t length, const void *data) {
    spiStartSend(driver, length, data);
}
static void spiStopTransferI(SPIDriver *driver, size_t *remaining) {
    (void)driver;
    (void)remaining;
}
#include "../../drivers/bluetooth/iton_bt.c"
void test_reset_sends(void) {
    iton_owner = ITON_IDLE;
    iton_head = 0;
    iton_count = 0;
    iton_pending_len = 0;
    iton_pending_keyboard = false;
    sends       = 0;
    sent_length = 0;
    memset(sent_bytes, 0, sizeof(sent_bytes));
}
unsigned test_sends(void) {
    return sends;
}
void test_advance_send(void) {
    iton_bt_data_cb(&SPID0);
    iton_bt_task();
}
size_t test_sent_length(void) {
    return sent_length;
}
uint8_t test_sent_byte(size_t index) {
    return sent_bytes[index];
}
void test_keyboard_reset(void) {
    iton_bt_send_kb_last_key = 0;
    test_reset_sends();
}
uint8_t test_keyboard_cached_key(void) {
    return iton_bt_send_kb_last_key;
}
void test_keyboard_send(uint8_t sixth, uint8_t mods, uint8_t first) {
    report_keyboard_t report = {0};
    report.keys[5]           = sixth;
    report.keys[0]           = first;
    report.mods              = mods;
    iton_bt_send_keyboard(&report);
}
