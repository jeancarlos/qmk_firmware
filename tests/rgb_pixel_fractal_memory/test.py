#!/usr/bin/env python3
"""Compare Pixel Fractal output and bound its static state on two matrix sizes."""
import pathlib
import re
import subprocess
import tempfile
import unittest

FIXTURE = pathlib.Path(__file__).with_name('fixture.c')


class PixelFractalTest(unittest.TestCase):
    def test_output_and_storage(self):
        for columns, expected in [(16, '016a1f85'), (18, '2d625225')]:
            with self.subTest(columns=columns), tempfile.TemporaryDirectory() as temp:
                binary = pathlib.Path(temp) / 'fixture'
                subprocess.run(['cc', '-std=c11', '-O0', f'-DMATRIX_COLS={columns}', str(FIXTURE), '-o', str(binary)], check=True)
                output = subprocess.check_output([binary], text=True).strip()
                self.assertEqual(output, expected)
                symbols = subprocess.check_output(['nm', '-S', str(binary)], text=True)
                match = re.search(r'^\w+\s+(\w+)\s+b\s+led\.\d+$', symbols, re.MULTILINE)
                self.assertIsNotNone(match)
                self.assertLessEqual(int(match.group(1), 16), 6 * (((columns + 1) // 2 + 7) // 8))


if __name__ == '__main__':
    unittest.main()
