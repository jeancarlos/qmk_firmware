from pathlib import Path
import re
import subprocess
import tempfile

keymap = (Path(__file__).resolve().parents[2] / 'keyboards/keychron/k2/rgb/v2/ansi/keymaps/default/keymap.c').read_text()
hook = re.search(r'void keyboard_post_init_user\(void\)\s*\{[^}]*\}', keymap)
assert hook
source = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
enum { D5 = 5, K2_BT_PROFILES = 3 };
static int state;
static uint32_t stored_profile;
static uint8_t k2_bt_profile;
static uint32_t eeconfig_read_user(void) { return stored_profile; }
static void k2_bt_load_profile(uint32_t profile) { if (profile < K2_BT_PROFILES) k2_bt_profile = profile; }
static __attribute__((unused)) void gpio_set_pin_input(int pin) { assert(pin == D5 && state == 0); state = 1; }
static __attribute__((unused)) void dip_switch_read(bool force) { assert(force && state == 1); state = 2; }
static void housekeeping_task_user(void) { assert(state == 2); state = 3; }
''' + hook.group() + r'''
int main(void) {
    stored_profile = UINT32_MAX;
    keyboard_post_init_user();
    assert(state == 3 && k2_bt_profile == 0);
    state = 0;
    stored_profile = 2;
    keyboard_post_init_user();
    assert(state == 3 && k2_bt_profile == 2);
}
'''
with tempfile.TemporaryDirectory() as tmp:
    executable = Path(tmp) / 'switch_test'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Werror', '-x', 'c', '-o', str(executable), '-'], input=source, text=True, check=True)
    subprocess.run([str(executable)], check=True)
