#!/usr/bin/env python3
"""Generate an original two-second pattern, or strictly check a PC4T v1 file."""
import argparse
from pathlib import Path
import re
import struct
import subprocess

def check(data):
    if len(data) < 24:
        raise ValueError('truncated header')
    magic, version, w, h, fps, count = struct.unpack_from('<4s5I', data)
    if (magic != b'PC4T' or version != 1 or not 0 < w <= 1024
            or not 0 < h <= 1024 or not 0 < fps <= 30 or not 0 < count <= fps * 5):
        raise ValueError('invalid PC4T header')
    at = 24
    for _ in range(count):
        if at + 4 > len(data):
            raise ValueError('truncated record')
        size, = struct.unpack_from('<I', data, at)
        at += 4
        if not 5 <= size <= 512000 or at + size > len(data):
            raise ValueError('invalid access-unit size')
        if not data[at:at+size].startswith((b'\0\0\0\1', b'\0\0\1')):
            raise ValueError('Annex-B prefix missing')
        at += size
    if at != len(data):
        raise ValueError('trailing data')
    return dict(width=w, height=h, fps=fps, access_units=count)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', type=Path)
    args = parser.parse_args()
    if args.check:
        print(check(args.check.read_bytes()))
        return
    raw = subprocess.check_output([
        'ffmpeg', '-hide_banner', '-loglevel', 'error', '-f', 'lavfi',
        '-i', 'testsrc2=size=448x448:rate=10:duration=2', '-an',
        '-c:v', 'libx264', '-profile:v', 'baseline', '-pix_fmt', 'yuv420p',
        '-preset', 'ultrafast', '-tune', 'zerolatency',
        '-x264-params', 'aud=1:repeat-headers=1:keyint=10', '-f', 'h264', 'pipe:1'])
    starts = [m.start() for m in re.finditer(b'\x00\x00\x00\x01\x09|\x00\x00\x01\x09', raw)]
    if not starts or starts[0] != 0 or len(starts) != 20:
        raise ValueError('unexpected ffmpeg AUD framing')
    ends = starts[1:] + [len(raw)]
    units = [raw[a:b] for a, b in zip(starts, ends)]
    data = struct.pack('<4s5I', b'PC4T', 1, 448, 448, 10, len(units))
    data += b''.join(struct.pack('<I', len(unit)) + unit for unit in units)
    print(check(data))
    output = Path(__file__).with_name('media') / 'pattern.pc4t'
    output.parent.mkdir(exist_ok=True)
    output.write_bytes(data)
    print(output)

if __name__ == '__main__':
    main()
