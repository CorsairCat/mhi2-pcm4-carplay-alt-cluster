#!/usr/bin/env python3
"""Expose executable LOAD bytes in a disposable ELF32 ARM disassembly copy.

Only the copy's section metadata changes. No loading, running or patching of
instructions. Output is an analysis artifact and must never be installed.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source', type=Path)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    original = args.source.read_bytes()
    if original[:7] != b'\x7fELF\x01\x01\x01' or struct.unpack_from('<H', original, 18)[0] != 40:
        raise ValueError('Expected ELF32 little-endian ARM')
    data = bytearray(original)
    phoff = struct.unpack_from('<I', data, 28)[0]
    stride, count = struct.unpack_from('<HH', data, 42)
    if stride < 32 or phoff + stride * count > len(data):
        raise ValueError('Invalid program table')
    segments = []
    for i in range(count):
        kind, offset, va, _, size, _, flags, _ = struct.unpack_from('<8I', data, phoff + stride * i)
        if kind == 1 and flags & 1:
            if size > len(data) - offset: raise ValueError('Invalid LOAD extent')
            segments.append((offset, va, size))
    if not segments: raise ValueError('No executable LOAD segment')
    # A separate name per segment prevents ambiguous section lookup.
    names, name_offsets = bytearray(b'\0'), []
    for i in range(len(segments)):
        name_offsets.append(len(names))
        names.extend(f'.analysis_code_{i}\0'.encode())
    shstr_name = len(names)
    names.extend(b'.shstrtab\0')
    names_at = len(data)
    data.extend(names)
    data.extend(bytes(-len(data) % 4))
    section_at = len(data)
    data.extend(bytes(40))
    for name_at, (offset, va, size) in zip(name_offsets, segments):
        data.extend(struct.pack('<10I', name_at, 1, 6, va, offset, size, 0, 0, 4, 0))
    data.extend(struct.pack('<10I', shstr_name, 3, 0, 0, names_at, len(names), 0, 0, 1, 0))
    struct.pack_into('<I', data, 32, section_at)
    struct.pack_into('<HHH', data, 46, 40, len(segments) + 2, len(segments) + 1)
    changed = {i for i in range(len(original)) if data[i] != original[i]}
    if not changed <= set(range(32, 36)) | set(range(46, 52)):
        raise ValueError('Unexpected modification outside section metadata')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('xb') as out: out.write(data)
    report = dict(source=str(args.source), source_sha256=hashlib.sha256(original).hexdigest(),
                  analysis_only=str(args.output), copy_sha256=hashlib.sha256(data).hexdigest(),
                  changed_original_offsets=sorted(changed), executable_segments=segments,
                  warning='For disassembly only; never execute or install this copy.')
    with args.output.with_suffix(args.output.suffix + '.json').open('x') as out:
        json.dump(report, out, indent=2)
        out.write('\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__': main()
