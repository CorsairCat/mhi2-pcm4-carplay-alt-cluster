"""Boundary and dialect checks for the read-only J9 instruction inspector."""
import struct
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from inspect_pcm4_hmi import ROM, instructions


class HMIReaderTests(unittest.TestCase):
    def test_truncated_and_unknown(self):
        for body in (b'\x11\x01', b'\xe7\x00\xb9', b'\xba', b'\xaa'):
            with self.subTest(body=body), self.assertRaises(ValueError):
                instructions(body, [])

    def test_little_endian_call(self):
        cp = [{'kind': 'word', 'value': i} for i in range(258)]
        rows = instructions(bytes.fromhex('b60101ac'), cp)
        self.assertEqual(rows[0]['constant']['value'], 257)
        self.assertEqual(rows[1]['operation'], 'return0')

    def test_interface_dialect(self):
        cp = [{'kind': 'member', 'name': 'switchContext'}]
        rows = instructions(bytes.fromhex('e700b90000ac'), cp)
        self.assertEqual(rows[0]['constant']['name'], 'switchContext')
        self.assertEqual(rows[1]['pc'], 5)

    def test_invalid_constant(self):
        with self.assertRaises(ValueError):
            instructions(bytes.fromhex('b60100'), [{}])

    def test_branch_into_operand_rejected(self):
        with self.assertRaises(ValueError):
            instructions(bytes.fromhex('a70100ac'), [])

    def test_backward_branch(self):
        rows = instructions(bytes.fromhex('00a7ffff'), [])
        self.assertEqual(rows[1]['target'], 0)

    def test_switch_targets(self):
        # tableswitch at pc 0: default and sole case target return at pc 20.
        body = b'\xaa\0\0\0' + struct.pack('<iiii', 20, 4, 4, 20) + b'\xac'
        rows = instructions(body, [])
        self.assertEqual(rows[0]['cases'], {'4': 20})
        self.assertEqual(rows[0]['default_target'], 20)
        self.assertEqual(rows[1]['pc'], 20)

    def test_switch_count_bounds(self):
        body = b'\xab\0\0\0' + struct.pack('<ii', 0, 0x7fffffff)
        with self.assertRaises(ValueError): instructions(body, [])

    def test_rom_extent(self):
        with self.assertRaises(ValueError): ROM(b'J99J' + bytes(43))


if __name__ == '__main__':
    unittest.main()
