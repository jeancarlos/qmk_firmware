from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
keymap = (root / 'keyboards/keychron/k2/rgb/v2/ansi/keymaps/default/keymap.c').read_text()
hook = re.search(r'static void k2_mode_rearm_keys\(void\)\s*\{.*?\n\}', keymap, re.S)
assert hook
source = r'''
#include <assert.h>
#include <stdint.h>
#define MATRIX_ROWS 6
typedef uint16_t matrix_row_t;
matrix_row_t matrix_previous[MATRIX_ROWS];
''' + hook.group() + r'''
int main(void) {
    for (int i = 0; i < MATRIX_ROWS; ++i) matrix_previous[i] = 0xffff;
    k2_mode_rearm_keys();
    for (int i = 0; i < 5; ++i) assert(matrix_previous[i] == 0);
    assert(matrix_previous[5] == (1u << 11));
}
'''
with tempfile.TemporaryDirectory() as tmp:
    executable = Path(tmp) / 'rearm_test'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Werror', '-x', 'c', '-o', str(executable), '-'], input=source, text=True, check=True)
    subprocess.run([str(executable)], check=True)
