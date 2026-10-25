#!/usr/bin/env python3
"""Recheck reviewed MU1476 NVIDIA argument accesses, without executing firmware.

Addresses are ELF virtual addresses in one hash-pinned reference, not process
addresses. Instruction matches reproduce review evidence, not a full ABI proof.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from compare_audi_interfaces import file_offset, symbols

ROOT = Path(__file__).resolve().parents[1]
CHECKS = [
    (0x27c8, 0xe3a02000, 'zero register used by stream-type comparison'),
    (0x27f8, 0xe5994010, 'open: load stream type at argument +16'),
    (0x2800, 0xe1540002, 'compare stream type with zero register'),
    (0x2810, 0x0a00007a, 'only zero branches to decoder-open path'),
    (0x2854, 0xe3a00116, 'nonzero stream type returns 0x80000005'),
    (0x278c, 0xe3a00a7d, 'input pool buffer allocation size 512000'),
    (0x2a64, 0xe5993000, 'open: output device at argument +0'),
    (0x2a68, 0xe3530001, 'output device compared with 1 before unsigned bound branch'),
    (0x2b4c, 0xe5d93009, 'open: byte at +9, use-layer candidate'),
    (0x2b5c, 0xe5993014, 'open: word at +20, semantics not fully traced'),
    (0x2b64, 0xe5d93008, 'open: byte at +8, priority candidate'),
    (0x2b6c, 0xe5d9300a, 'open: byte at +10, layer candidate'),
    (0x1ebc, 0xedd17a02, 'configure: float at +8'),
    (0x1ef8, 0xe5973000, 'configure: word at +0'),
    (0x1f00, 0xe5973004, 'configure: word at +4'),
    (0x1a74, 0xe5968004, 'decode: length at +4'),
    (0x1b54, 0xe5961000, 'decode: data pointer at +0'),
    (0x1b68, 0xedd60b02, 'decode: 64-bit timestamp at +8'),
    (0x1b78, 0xe5d62010, 'decode: byte flag at +16'),
    (0x175c, 0xe5d53008, 'attributes: geometry-update flag at +8'),
    (0x18e8, 0xe595300c, 'attributes: rectangle words start at +12'),
    (0x1900, 0xe5953018, 'attributes: fourth rectangle word at +24'),
    (0x1918, 0xe5953024, 'attributes: seventh rectangle word at +36'),
    (0x1774, 0xe5d5003c, 'attributes: separate command flag at +60'),
    (0xe424, 0xebffcbca, 'window-manager open calls kdInitializeNV'),
    (0xe3a8, 0xebffcbf8, 'window-manager close calls kdTerminateNV'),
    (0xe4d0, 0xebffcb90, 'window-manager open calls kdCreateStreamNV'),
    (0xe4e4, 0xe3011789, 'integer property ID 0x1789'),
    (0xe4e8, 0xebffcb69, 'sets property from stored layer word'),
    (0xe308, 0xe301178f, 'boolean property ID 0x178f'),
    (0xe30c, 0xebffcc22, 'sets boolean property true on layer-validity path'),
]


def plt_imports(data):
    """Resolve this reviewed ARM PLT, checking each thunk's actual GOT address."""
    phoff = struct.unpack_from('<I', data, 28)[0]
    entsize, count = struct.unpack_from('<HH', data, 42)
    tags = {}
    for i in range(count):
        kind, off, _, _, size = struct.unpack_from('<5I', data, phoff + i * entsize)
        if kind != 2: continue
        for at in range(off, off + size, 8):
            tag, value = struct.unpack_from('<II', data, at)
            if not tag: break
            tags[tag] = value
    strings, symbols_at = file_offset(data, tags[5]), file_offset(data, tags[6])
    relocations = file_offset(data, tags[23])
    found = {}
    def immediate(word):
        value, rotation = word & 255, ((word >> 8) & 15) * 2
        return ((value >> rotation) | (value << ((32 - rotation) % 32))) & 0xffffffff
    for i in range(tags[2] // 8):
        got, info = struct.unpack_from('<II', data, relocations + i * 8)
        address = 0x1264 + 12 * i  # reviewed .plt start and stride for this hash
        first, second, third = struct.unpack_from('<3I', data, file_offset(data, address))
        if (info & 255 != 22 or first & 0xfffff000 != 0xe28fc000 or
            second & 0xfffff000 != 0xe28cc000 or third & 0xfffff000 != 0xe5bcf000):
            raise ValueError('Unrecognized PLT relocation/thunk')
        target = (address + 8 + immediate(first) + immediate(second) + (third & 4095)) & 0xffffffff
        if target != got: raise ValueError('PLT/GOT mapping mismatch')
        name = struct.unpack_from('<I', data, symbols_at + (info >> 8) * 16)[0]
        end = data.index(b'\0', strings + name)
        found[address] = data[strings + name:end].decode('ascii')
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library', type=Path, default=ROOT / 'private/analysis/mu1476-aio-app50/files/armle/lib/libnvss_video.so')
    parser.add_argument('--report', type=Path, default=ROOT / 'private/analysis/mu1476-display-port/abi-audit.json')
    parser.add_argument('--kd', type=Path, default=ROOT / 'private/analysis/mu1476-aio-mifs50/files/lib/libKD.so')
    parser.add_argument('--nvmedia', type=Path, default=ROOT / 'private/analysis/mu1476-nvmedia/files/armle/usr/lib/libnvmedia.so')
    args = parser.parse_args()
    profile = json.loads((ROOT / 'profiles/pcm4-mu1476-display.json').read_text())
    data = args.library.read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    if digest != profile['nvss_sha256']:
        raise SystemExit('Reference hash mismatch; refusing to interpret fixed addresses.')
    checked = []
    for address, expected, meaning in CHECKS:
        offset = file_offset(data, address)
        observed = struct.unpack_from('<I', data, offset)[0]
        checked.append(dict(address=hex(address), file_offset=hex(offset),
                            expected=hex(expected), observed=hex(observed),
                            matches=observed == expected, interpretation=meaning))
    exports = symbols(args.library)
    imports = plt_imports(data)
    calls = {}
    for address, expected in ((0xe424, 'kdInitializeNV'), (0xe3a8, 'kdTerminateNV'),
                              (0xe4d0, 'kdCreateStreamNV'), (0xe4e8, 'kdSetStreamPropertyivNV'),
                              (0xe30c, 'kdSetStreamPropertybvNV')):
        word = struct.unpack_from('<I', data, file_offset(data, address))[0]
        displacement = word & 0xffffff
        if displacement & 0x800000: displacement -= 0x1000000
        target = (address + 8 + displacement * 4) & 0xffffffff
        observed = imports.get(target)
        calls[hex(address)] = dict(plt_target=hex(target), expected=expected, observed=observed,
                                   matches=observed == expected)
    names = 'NvSSVideoOpen NvSSVideoClose NvSSVideoStreamConfigure NvSSVideoGetAttribs NvSSVideoSetAttribs NvSSVideoDecode'.split()
    passed = all(c['matches'] for c in checked) and all(n in exports for n in names) and all(c['matches'] for c in calls.values())
    got_base = (0x2700 + struct.unpack_from('<I', data, file_offset(data, 0x2c8c))[0]) & 0xffffffff
    message_va = (got_base + struct.unpack_from('<I', data, file_offset(data, 0x2ca4))[0]) & 0xffffffff
    message_at = file_offset(data, message_va)
    message = data[message_at:data.index(b'\0', message_at)].decode('ascii')
    passed = passed and message == 'Invalid Stream Type'
    additional = {}
    for name, path, key, checks in (
        ('kd', args.kd, 'kd_sha256', (
            (0x7c28, 0xe3530000), (0x7c2c, 0x0a000001), (0x7c30, 0xe3a00000),
            (0x7c34, 0xe8bd8010), (0x7b18, 0xe5869000), (0x7ac0, 0xe5933240),
            (0x7acc, 0xeaffffb9), (0x7a84, 0xe5832000))),
        ('nvmedia', args.nvmedia, 'nvmedia_sha256', (
            (0x7cb4, 0xea0002b6), (0x87b8, 0xebffe65d), (0x8800, 0xe2413cb3),
            (0x8834, 0xe5057018), (0x888c, 0xe5058018), (0x8894, 0xe505a018)))):
        raw = path.read_bytes()
        sha = hashlib.sha256(raw).hexdigest()
        if sha != profile[key]:
            raise SystemExit(f'{name} hash mismatch; refusing fixed-address checks')
        rows = [dict(address=hex(address), expected=hex(expected),
                     observed=hex(struct.unpack_from('<I', raw, file_offset(raw, address))[0]))
                for address, expected in checks]
        ok = all(row['expected'] == row['observed'] for row in rows)
        additional[name] = dict(path=str(path), sha256=sha, checks=rows, passed=ok)
        passed = passed and ok
    report = dict(reference=str(args.library), sha256=digest,
                  checked_instruction_count=len(checked), checks=checked,
                  exports={n: hex(exports[n]) if n in exports else None for n in names},
                  stream_type_error=dict(address=hex(message_va), text=message),
                  window_manager_import_calls=calls,
                  additional_libraries=additional,
                  reviewed_checks_passed=passed, deployment_ready=False,
                  caveat='Static reviewed access checks only. No runtime, full ABI, visibility or output mapping validation.')
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(f'{len(checked)} reviewed instructions: {"PASS" if passed else "FAIL"}; deployment_ready=false')
    print('Additional checks: ' + ', '.join(f'{name} {len(value["checks"])}' for name, value in additional.items()))
    print(f'Report: {args.report}')
    if not passed:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
