#!/usr/bin/env python3
"""Static ARM32/QNX loader/import checks against a supplied vehicle libc.

This is an import-name/ELF gate, not runtime or complete symbol-version proof.
Never executes the target binary or the vendor library.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from compare_audi_interfaces import file_offset, symbols


def inspect(path):
    data = path.read_bytes()
    file_offset(data, struct.unpack_from('<I', data, 24)[0])
    phoff = struct.unpack_from('<I', data, 28)[0]
    size, count = struct.unpack_from('<HH', data, 42)
    tags, interpreter = {}, None
    for i in range(count):
        kind, offset, _, _, length = struct.unpack_from('<5I', data, phoff + i * size)
        if offset + length > len(data):
            raise ValueError('program header outside file')
        if kind == 3:
            interpreter = data[offset:offset + length].rstrip(b'\0').decode('ascii')
        if kind == 2:
            for at in range(offset, offset + length, 8):
                tag, value = struct.unpack_from('<II', data, at)
                if not tag:
                    break
                tags.setdefault(tag, []).append(value)
    strings = file_offset(data, tags[5][0])
    string_size = tags[10][0]

    def name(index):
        if index >= string_size:
            raise ValueError('dynamic string outside table')
        start = strings + index
        end = data.index(b'\0', start, strings + string_size)
        return data[start:end].decode('ascii')

    if tags[11][0] != 16:
        raise ValueError('unsupported dynamic symbol size')
    hash_at = file_offset(data, tags[4][0])
    nchain = struct.unpack_from('<I', data, hash_at + 4)[0]
    sym_at = file_offset(data, tags[6][0])
    if nchain > (len(data) - sym_at) // 16:
        raise ValueError('dynamic symbols outside file')
    strong, weak = [], []
    for i in range(nchain):
        index, _, _, info, _, section = struct.unpack_from('<IIIBBH', data, sym_at + i * 16)
        if index and not section and info >> 4 in (1, 2):
            (weak if info >> 4 == 2 else strong).append(name(index))
    return dict(sha256=hashlib.sha256(data).hexdigest(), interpreter=interpreter,
                elf_type=struct.unpack_from('<H', data, 16)[0],
                flags=struct.unpack_from('<I', data, 36)[0],
                needed=[name(i) for i in tags.get(1, [])],
                strong_imports=sorted(set(strong)), weak_imports=sorted(set(weak)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('--libc', type=Path, required=True)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--dependency', type=Path, action='append', default=[])
    parser.add_argument('--shared', action='store_true')
    args = parser.parse_args()
    result = inspect(args.binary)
    exports = symbols(args.libc)
    provided = set(exports)
    result['dependencies'] = {}
    for dependency in args.dependency:
        provided.update(symbols(dependency))
        result['dependencies'][dependency.name] = hashlib.sha256(dependency.read_bytes()).hexdigest()
    result['libc_sha256'] = hashlib.sha256(args.libc.read_bytes()).hexdigest()
    result['missing_strong_imports_in_libc'] = sorted(set(result['strong_imports']) - exports.keys())
    result['missing_strong_imports'] = sorted(set(result['strong_imports']) - provided)
    allowed = {args.libc.name} | {p.name for p in args.dependency}
    # Every additional provider is explicit; default remains libc-only.
    result['static_gate_passes'] = bool(
        result['interpreter'] == (None if args.shared else '/usr/lib/ldqnx.so.2')
        and result['elf_type'] == (3 if args.shared else 2) and result['flags'] >> 24 == 5
        and not result['flags'] & 0x400  # hard-float ABI marker
        and bool(result['needed']) and set(result['needed']) <= allowed
        and not result['missing_strong_imports'])
    result['vehicle_runtime_verified'] = False
    text = json.dumps(result, indent=2)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(text + '\n')
    print(text)
    return 0 if result['static_gate_passes'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
