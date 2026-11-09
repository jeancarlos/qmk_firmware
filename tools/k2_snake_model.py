"""Host-only behavioural model; not keyboard firmware or an uploadable package."""
import colorsys
import json
from pathlib import Path


class Snake:
    LENGTH = 8

    def __init__(self, neighbors, body, rng):
        if len(body) != self.LENGTH or len(set(body)) != self.LENGTH:
            raise ValueError('body must contain eight distinct keys')
        if any(not 0 <= led < len(neighbors) for led in body):
            raise ValueError('body key outside graph')
        self.neighbors = neighbors
        self.body = list(body)
        self.rng = rng
        self.food = bytearray((len(neighbors) + 7) // 8)
        self.phase = 0

    def food_at(self, led):
        return 0 <= led < len(self.neighbors) and bool(self.food[led // 8] & (1 << (led % 8)))

    def press(self, led):
        if 0 <= led < len(self.neighbors) and led not in self.body:
            self.food[led // 8] |= 1 << (led % 8)

    def press_matrix(self, matrix, matrices):
        try:
            led = matrices.index(tuple(matrix))
        except ValueError:
            return False
        self.press(led)
        return True

    def step(self):
        candidates = [led for led in self.neighbors[self.body[0]]
                      if 0 <= led < len(self.neighbors) and led not in self.body[:-1]]
        if not candidates:
            candidates = [led for led in self.neighbors[self.body[-1]]
                          if 0 <= led < len(self.neighbors) and led not in self.body[1:]]
            if not candidates:
                return
            self.body.reverse()
        led = self.rng.choice(candidates)
        self.food[led // 8] &= ~(1 << (led % 8))
        self.body = [led] + self.body[:-1]
        self.phase = (self.phase + 1) % 256

    def brightness(self, led):
        try:
            return 255 * (self.LENGTH - self.body.index(led)) // self.LENGTH
        except ValueError:
            return 255 if self.food_at(led) else 0

    def color(self, led):
        if led in self.body:
            index = self.body.index(led)
            rgb = colorsys.hsv_to_rgb(((self.phase * 4 + index * 32) % 256) / 256, 1, self.brightness(led) / 255)
            return tuple(round(component * 255) for component in rgb)
        return (255, 255, 255) if self.food_at(led) else (0, 0, 0)


def k2_layout():
    """Build a host-only graph from the target's configured LED positions."""
    path = (Path(__file__).resolve().parents[1] /
            'keyboards/keychron/k2/rgb/v2/ansi/keyboard.json')
    layout = json.loads(path.read_text())['rgb_matrix']['layout']
    rows = {}
    for i, led in enumerate(layout):
        rows.setdefault(led['y'], []).append(i)
    ys = sorted(rows)
    graph = [set() for _ in layout]

    def connect(a, b):
        graph[a].add(b)
        graph[b].add(a)

    for row in rows.values():
        row.sort(key=lambda i: layout[i]['x'])
        for a, b in zip(row, row[1:]):
            connect(a, b)
    for y1, y2 in zip(ys, ys[1:]):
        for source, destination in ((rows[y1], rows[y2]), (rows[y2], rows[y1])):
            for a in source:
                b = min(destination, key=lambda i: abs(layout[a]['x'] - layout[i]['x']))
                connect(a, b)
    body = list(reversed(rows[ys[0]][:8]))
    return [tuple(sorted(neighbors)) for neighbors in graph], [tuple(led['matrix']) for led in layout], body
