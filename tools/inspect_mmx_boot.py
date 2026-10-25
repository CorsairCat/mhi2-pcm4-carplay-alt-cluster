#!/usr/bin/env python3
"""Read-only inspection of the supplied MMX zlib boot wrappers.

Format lead: harman-f/MHI2-MHI2Q-MH2P-Firmware-Toolkit,
docs/MHI2_FLASH_LAYOUT_AND_RECOVERY.md. No execution or image modification.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zlib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--select', default=r'(?i)(display|cap_app|\.conf$)', help='Regex over regular-file paths to extract')
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError('Choose a new output directory')
    raw = args.source.read_bytes()
    if raw[:8] != bytes.fromhex('41ff44ff4fff44ff'):
        raise ValueError('Unknown wrapper')
    length = struct.unpack_from('<I', raw, 8)[0]
    page = struct.unpack_from('<I', raw, 36)[0]
    if page < 40 or page + length > len(raw):
        raise ValueError('Invalid wrapper extent')
    decoder = zlib.decompressobj()
    expanded = decoder.decompress(raw[page:page + length], 64 * 1024**2)
    if not decoder.eof or decoder.unconsumed_tail:
        raise ValueError('Truncated or oversized zlib stream')
    if expanded[8:12] != bytes.fromhex('eb7eff00'):
        raise ValueError('Unknown nested startup header')
    offset = 8 + struct.unpack_from('<I', expanded, 8 + 0x20)[0]
    image = expanded[offset:]
    if image[:7] != b'imagefs':
        raise ValueError('No ImageFS at startup-declared boundary')
    size, end, cursor = struct.unpack_from('<III', image, 8)
    if size > len(image) or size % 4 or not 0 < cursor < end < size:
        raise ValueError('Invalid ImageFS geometry')
    image = image[:size]
    checksum = sum(v[0] for v in struct.iter_unpack('<I', image)) & 0xffffffff
    if checksum:
        raise ValueError('ImageFS checksum failure')
    args.output.mkdir(parents=True)
    entries = []
    while cursor + 24 <= end:
        length, _, inode, mode, _, _, _ = struct.unpack_from('<HHIIIII', image, cursor)
        if not length:
            break
        if length < 24 or cursor + length > end:
            raise ValueError('Invalid directory record')
        at = cursor + 24
        kind = mode & 0xf000
        entry = dict(inode=inode, mode=mode)
        content = None
        if kind == 0x8000:
            pos, n = struct.unpack_from('<II', image, at)
            at += 8
            if not end <= pos <= pos + n <= size:
                raise ValueError('Invalid regular-file extent')
            content = image[pos:pos+n]
            entry.update(size=n, offset=pos, sha256=hashlib.sha256(content).hexdigest())
        elif kind == 0xa000:
            at += 4  # Inventory only; never create a symlink.
        elif kind != 0x4000:
            raise ValueError('Unknown record type')
        name = image[at:cursor+length].split(b'\0', 1)[0].decode()
        path = Path(name)
        if path.is_absolute() or '..' in path.parts or '\\' in name:
            raise ValueError('Unsafe path')
        entry['name'] = name
        entries.append(entry)
        if content is not None and re.search(args.select, name):
            dest = args.output / 'files' / path
            dest.parent.mkdir(parents=True, exist_ok=True)
            with dest.open('xb') as stream:
                stream.write(content)
        cursor += length
    report = dict(source=str(args.source), source_sha256=hashlib.sha256(raw).hexdigest(),
                  expanded_bytes=len(expanded), imagefs_offset=offset,
                  imagefs_bytes=size, checksum_sum=checksum, entries=entries)
    (args.output / 'inventory.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v if k != 'entries' else len(v) for k, v in report.items()}, indent=2))


if __name__ == '__main__':
    main()
