import random
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from k2_snake_model import Snake


class SnakeTests(unittest.TestCase):
    def setUp(self):
        self.graph = [((i - 1) % 12, (i + 1) % 12) for i in range(12)]
        self.snake = Snake(self.graph, list(reversed(range(8))), random.Random(4))

    def test_press_marks_key_until_eaten(self):
        self.snake.press(8)
        self.assertTrue(self.snake.food_at(8))
        self.snake.step()
        self.assertEqual(self.snake.body[0], 8)
        self.assertFalse(self.snake.food_at(8))
        self.assertEqual(len(self.snake.body), 8)

    def test_body_is_safe_zone(self):
        for led in self.snake.body:
            self.snake.press(led)
            self.assertFalse(self.snake.food_at(led))

    def test_multiple_food_and_invalid_indices(self):
        self.snake.press(8)
        self.snake.press(9)
        self.snake.press(8)
        self.snake.press(-1)
        self.snake.press(12)
        self.snake.step()
        self.assertFalse(self.snake.food_at(8))
        self.assertTrue(self.snake.food_at(9))

    def test_steps_are_adjacent_and_body_remains_bounded(self):
        for _ in range(1000):
            previous = self.snake.body[0]
            self.snake.step()
            self.assertIn(self.snake.body[0], self.graph[previous])
            self.assertEqual(len(self.snake.body), 8)
            self.assertEqual(len(set(self.snake.body)), 8)

    def test_fade_and_safe_zone_include_tail(self):
        self.assertEqual(self.snake.brightness(self.snake.body[0]), 255)
        self.assertGreater(self.snake.brightness(self.snake.body[-1]), 0)
        values = [self.snake.brightness(led) for led in self.snake.body]
        self.assertEqual(values, sorted(values, reverse=True))
        self.assertEqual(self.snake.brightness(10), 0)

    def test_snake_is_rgb_and_food_remains_lit(self):
        self.snake.press(8)
        self.assertNotEqual(self.snake.color(self.snake.body[0]), (0, 0, 0))
        self.assertNotEqual(self.snake.color(self.snake.body[0]), self.snake.color(self.snake.body[1]))
        self.assertNotEqual(self.snake.color(8), (0, 0, 0))
        self.assertEqual(self.snake.color(9), (0, 0, 0))

    def test_dead_end_does_not_hang(self):
        snake = Snake([() for _ in range(8)], list(range(8)), random.Random(1))
        previous = snake.body[:]
        snake.step()
        self.assertEqual(snake.body, previous)



class K2LayoutTests(unittest.TestCase):
    def test_k2_layout_graph_is_connected_and_uses_real_leds(self):
        from k2_snake_model import k2_layout
        graph, matrices, body = k2_layout()
        self.assertEqual(len(graph), 84)
        self.assertEqual(len(matrices), 84)
        self.assertEqual(len(set(matrices)), 84)
        self.assertEqual(len(body), 8)
        self.assertTrue(all(body[i + 1] in graph[body[i]] for i in range(7)))
        seen, pending = {0}, [0]
        while pending:
            for led in graph[pending.pop()]:
                if led not in seen:
                    seen.add(led)
                    pending.append(led)
        self.assertEqual(len(seen), 84)
        self.assertTrue(all(i in graph[j] for i, ns in enumerate(graph) for j in ns))
        self.assertIn(1, graph[0])
        self.assertIn(16, graph[0])
        self.assertNotIn(15, graph[0])

    def test_matrix_presses_and_motion_on_k2_layout(self):
        from k2_snake_model import k2_layout
        graph, matrices, body = k2_layout()
        snake = Snake(graph, body, random.Random(15))
        self.assertTrue(snake.press_matrix(matrices[30], matrices))
        self.assertTrue(snake.food_at(30))
        self.assertFalse(snake.press_matrix((99, 99), matrices))
        for _ in range(500):
            previous = snake.body[:]
            snake.step()
            if snake.body != previous:
                # The direction may reverse in a cul-de-sac without moving the body.
                self.assertTrue(snake.body[0] in graph[previous[0]] or
                                snake.body[0] in graph[previous[-1]])
            self.assertEqual(len(set(snake.body)), 8)

    def test_turns_around_when_head_is_trapped(self):
        graph = [() for _ in range(10)]
        for a, b in zip(range(8), range(1, 8)):
            graph[a] += (b,)
            graph[b] += (a,)
        graph[7] += (8,)
        graph[8] += (7,)
        snake = Snake(graph, list(range(8)), random.Random(1))
        snake.step()
        self.assertEqual(snake.body[0], 8)
        self.assertEqual(snake.body[1:], [7, 6, 5, 4, 3, 2, 1])


if __name__ == '__main__':
    unittest.main()
