"""Compare the compact host experiment against the Python reference on the K2 graph."""
import ctypes
import random
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from k2_snake_model import Snake, k2_layout


class State(ctypes.Structure):
    _fields_ = [('body', ctypes.c_uint8 * 8), ('food', ctypes.c_uint8 * 11)]


class CompactTest(unittest.TestCase):
    def test_k2_behavior_matches_reference(self):
        graph, _, body = k2_layout()
        with tempfile.TemporaryDirectory() as tmp:
            callback = Path(tmp) / 'graph.c'
            rows = ',\n'.join('{' + ','.join(map(str, list(row) + [255] * (5 - len(row)))) + '}' for row in graph)
            callback.write_text('#include <stdint.h>\nstatic const uint8_t g[84][5] = {' + rows + '};\nuint8_t snake_neighbor(uint8_t led,uint8_t slot){return g[led][slot];}\n')
            so = Path(tmp) / 'snake.so'
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared', '-fPIC', '-I', str(ROOT / 'tools'), str(ROOT / 'tools/k2_snake_compact.c'), str(callback), '-o', str(so)], check=True)
            lib = ctypes.CDLL(str(so))
            lib.snake_init.argtypes = [ctypes.POINTER(State), ctypes.POINTER(ctypes.c_uint8)]
            lib.snake_press.argtypes = [ctypes.POINTER(State), ctypes.c_uint8]
            lib.snake_step.argtypes = [ctypes.POINTER(State), ctypes.c_uint8]
            lib.snake_step.restype = ctypes.c_bool
            state = State()
            lib.snake_init(ctypes.byref(state), (ctypes.c_uint8 * 8)(*body))
            class Pick:
                value = 0
                def choice(self, candidates):
                    return candidates[self.value % len(candidates)]
            pick = Pick()
            reference = Snake(graph, body, pick)
            rng = random.Random(42)
            for _ in range(1000):
                led = rng.randrange(84)
                lib.snake_press(ctypes.byref(state), led)
                reference.press(led)
                pick.value = rng.randrange(256)
                lib.snake_step(ctypes.byref(state), pick.value)
                reference.step()
                self.assertEqual(list(state.body), reference.body)
                self.assertEqual(bytes(state.food), bytes(reference.food))


if __name__ == '__main__':
    unittest.main()
