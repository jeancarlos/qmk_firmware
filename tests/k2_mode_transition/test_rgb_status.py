from pathlib import Path
import re
import subprocess
import tempfile

keymap = (Path(__file__).resolve().parents[2] / 'keyboards/keychron/k2/rgb/v2/ansi/keymaps/default/keymap.c').read_text()
hook = re.search(r'(?ms)^bool rgb_matrix_render_status_kb\(void\) \{.*?^\}', keymap)
assert hook
source = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

enum { CONNECTION_HOST_USB, CONNECTION_HOST_BLUETOOTH };
enum { K2_BT_PROFILES = 3, K2_BT_NORMAL_RGB = UINT8_MAX, RGB_MATRIX_MAXIMUM_BRIGHTNESS = 200 };
static int host, k2_mode_desired, updates, indicator;
static uint8_t status;
static const struct { uint8_t matrix_co[2][4]; } g_led_config = {.matrix_co = {{0}, {0, 2, 4, 5}}};

static int connection_get_host(void) { return host; }
static uint8_t k2_bt_status_led(void) { return status; }
static void rgb_matrix_set_color_all(uint8_t r, uint8_t g, uint8_t b) {
    assert(r == 0 && g == 0 && b == 0);
    indicator = 0;
}
static void rgb_matrix_set_color(uint8_t led, uint8_t r, uint8_t g, uint8_t b) {
    assert(r == RGB_MATRIX_MAXIMUM_BRIGHTNESS && g == r && b == r);
    assert(indicator == 0);
    indicator = led;
}
static void rgb_matrix_update_pwm_buffers(void) { ++updates; }
''' + hook.group() + r'''
int main(void) {
    assert(!rgb_matrix_render_status_kb() && updates == 0);
    host = CONNECTION_HOST_BLUETOOTH;
    status = 3;
    assert(rgb_matrix_render_status_kb() && updates == 1 && indicator == 5);
    assert(rgb_matrix_render_status_kb() && updates == 1);
    status = 0;
    assert(rgb_matrix_render_status_kb() && updates == 2 && indicator == 0);
    status = K2_BT_NORMAL_RGB;
    assert(!rgb_matrix_render_status_kb() && updates == 2);
    status = 1;
    assert(rgb_matrix_render_status_kb() && updates == 3 && indicator == 2);
    host = CONNECTION_HOST_USB;
    assert(!rgb_matrix_render_status_kb() && updates == 3);
}
'''
with tempfile.TemporaryDirectory() as tmp:
    executable = Path(tmp) / 'rgb_status_test'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-x', 'c', '-o', str(executable), '-'], input=source, text=True, check=True)
    subprocess.run([str(executable)], check=True)
