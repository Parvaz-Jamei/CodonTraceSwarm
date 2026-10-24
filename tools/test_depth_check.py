"""Stub-decoder tests for tools/depth_check.py: python3 -m unittest tools/test_depth_check.py"""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from depth_check import deep_envelope, depth64_rejected, golden_reading  # noqa: E402


def accept_all(raw):
    return {}


def reject_value(raw):
    raise ValueError('JSON nesting too deep')


def raise_type(raw):
    raise TypeError('unexpected list')


def recurse(raw):
    raise RecursionError('maximum recursion depth exceeded')


class DepthCheck(unittest.TestCase):
    def test_envelope_value_nesting(self):
        import json
        v = json.loads(deep_envelope(golden_reading()))['payload']['value']
        depth = 0
        while isinstance(v, list):
            v, depth = v[0], depth + 1
        # depth=64 counts the scalar as the innermost level, so 63 lists wrap it.
        self.assertEqual(depth, 63)

    def test_decoder_that_accepts_everything_fails(self):
        self.assertFalse(depth64_rejected(accept_all, golden_reading())[0])

    def test_value_error_passes(self):
        self.assertTrue(depth64_rejected(reject_value, golden_reading())[0])

    def test_type_error_is_not_a_rejection(self):
        self.assertFalse(depth64_rejected(raise_type, golden_reading())[0])

    def test_recursion_error_is_not_a_rejection(self):
        self.assertFalse(depth64_rejected(recurse, golden_reading())[0])


if __name__ == '__main__':
    unittest.main()
