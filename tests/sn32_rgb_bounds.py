#!/usr/bin/env python3
"""Exercise the actual COL2ROW hardware-PWM scanner with the K2's empty cells."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(os.environ.get("QMK_ROOT", Path(__file__).resolve().parent.parent))
source = (root / "drivers/led/sn32f2xx.c").read_text()
start = source.index("static void update_pwm_channels(PWMDriver *pwmp) {")
end = source.index("\n#elif (SN32F2XX_PWM_DIRECTION == ROW2COL)", start)
function = source[start:end]
stub = r'''
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#define HARDWARE_PWM 0
#define SOFTWARE_PWM 1
#define SN32F2XX_PWM_CONTROL HARDWARE_PWM
#define SN32F2XX_RGB_MATRIX_COLS 16
#define SN32F2XX_RGB_MATRIX_ROWS 6
#define SN32F2XX_RGB_MATRIX_ROW_CHANNELS 3
#define SN32F2XX_RGB_MATRIX_ROWS_HW 18
#define SN32F2XX_LED_COUNT 84
#define ROWS_PER_HAND 6
#define SN32F2XX_RGB_OUTPUT_ACTIVE_LEVEL 1
#define SN32F2XX_RGB_OUTPUT_ACTIVE_HIGH 1
#define SN32F2XX_RGB_OUTPUT_ACTIVE_LOW 0
typedef struct { uint8_t r, g, b; } RGB;
typedef int PWMDriver;
static RGB led_state[84];
static uint8_t current_row, current_key_row, chan_col_order[16], led_row_pins[18];
static struct { uint8_t matrix_co[6][16]; } g_led_config;
static unsigned writes, disabled;
static void shared_matrix_rgb_disable_output(void) { disabled += 16; }
static void gpio_write_pin_high(int pin) { (void)pin; }
static void pwmEnableChannel(PWMDriver *p, int ch, int value) {
    (void)p; (void)ch; assert(value == 42); writes++;
}
static void pwmDisableChannel(PWMDriver *p, int ch) { (void)p; (void)ch; disabled++; }
'''
main = r'''
int main(void) {
    PWMDriver driver = 0;
    for (int r = 0; r < 6; r++)
        for (int c = 0; c < 16; c++) g_led_config.matrix_co[r][c] = 255;
    g_led_config.matrix_co[0][0] = 0;
    led_state[0].g = 42;
    update_pwm_channels(&driver);
    assert(writes == 1);
    assert(disabled == 16);
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cfile, binary = Path(tmp) / "test.c", Path(tmp) / "test"
    cfile.write_text(stub + function + main)
    subprocess.run(["clang", "-std=c11", "-fsanitize=bounds", "-fsanitize-trap=bounds", "-g", str(cfile), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
