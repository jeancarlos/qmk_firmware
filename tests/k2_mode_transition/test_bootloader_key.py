from pathlib import Path
import re
import subprocess
import tempfile

keymap = (Path(__file__).resolve().parents[2] / "keyboards/keychron/k2/rgb/v2/ansi/keymaps/default/keymap.c").read_text()
for layer in ("WIN_FN", "MAC_FN"):
    assert re.search(r"\[" + layer + r"\]\s*=\s*LAYOUT_ansi\s*\(\s*(?:/\*.*?\*/\s*)?QK_BOOT\b", keymap, re.S), layer

hook = re.search(r"bool pre_process_record_user\([^)]*\)\s*\{.*?^\}", keymap, re.S | re.M)
assert hook
source = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
typedef struct { struct { bool pressed; } event; } keyrecord_t;
enum { WIN_FN = 1, MAC_FN = 3, QK_BOOT = 0x7c00, KC_1 = 0x1e, KC_3 = 0x20 };
#define MO(layer) (0x5200 | (layer))
static bool blocked, bluetooth, fn;
static bool k2_bt_shortcuts_active(void) { return bluetooth && !blocked; }
static bool k2_fn_active(void) { return fn; }
static bool k2_bt_profile_key(uint8_t profile, bool pressed, bool shortcut) {
    static bool consume_release;
    assert(profile == 0);
    if (pressed) return consume_release = shortcut;
    bool consumed = consume_release;
    consume_release = false;
    return consumed;
}
static bool k2_mode_blocks_keys(void) { return blocked; }
''' + hook.group() + r'''
int main(void) {
    keyrecord_t record = {0};
    blocked = true;
    for (int pressed = 0; pressed < 2; ++pressed) {
        record.event.pressed = pressed;
        assert(pre_process_record_user(MO(WIN_FN), &record));
        assert(pre_process_record_user(MO(MAC_FN), &record));
        assert(pre_process_record_user(QK_BOOT, &record));
        assert(!pre_process_record_user(4, &record));
    }
    blocked = false;
    assert(pre_process_record_user(4, &record));
    fn = true;
    assert(pre_process_record_user(KC_1, &record)); /* Fn+1 is transparent on USB. */
    bluetooth = true;
    assert(!pre_process_record_user(KC_1, &record));
    record.event.pressed = false;
    fn = false;
    assert(!pre_process_record_user(KC_1, &record)); /* Release stays consumed. */
    assert(pre_process_record_user(KC_1, &record));
}
'''
with tempfile.TemporaryDirectory() as tmp:
    executable = Path(tmp) / "bootloader_test"
    subprocess.run(["cc", "-std=c11", "-Wall", "-Werror", "-x", "c", "-o", str(executable), "-"], input=source, text=True, check=True)
    subprocess.run([str(executable)], check=True)
