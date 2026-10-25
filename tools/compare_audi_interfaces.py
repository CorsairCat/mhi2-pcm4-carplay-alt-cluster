#!/usr/bin/env python3
"""Static interface checks for the Audi MU1438 strategy against local PCM4 files.

Reports symbol/prologue presence only, never runtime compatibility. Does not
execute, patch or redistribute vendor code. Run with the workspace as cwd.
Reference: aberbic/Audi-MHI2-Virtual-Cockpit-Altscreen-CarPlay-AndroidAuto,
carplay/src/native/libaltscreen111_mu1438.c and cluster_runtime.c (2026-10-24).
"""
import hashlib
import json
from pathlib import Path
import struct


def symbols(path):
    # QNX ImageFS libraries can have no useful section headers. Read the
    # loader's dynamic table directly instead of trusting objdump -T output.
    data = path.read_bytes()
    phoff = struct.unpack_from('<I', data, 28)[0]
    entsize, count = struct.unpack_from('<HH', data, 42)
    dynamic = None
    for i in range(count):
        kind, offset, _, _, filesz = struct.unpack_from('<5I', data, phoff + i * entsize)
        if kind == 2:
            dynamic = (offset, filesz)
    if dynamic is None:
        raise ValueError('No dynamic table')
    tags = {}
    for at in range(dynamic[0], sum(dynamic), 8):
        tag, value = struct.unpack_from('<II', data, at)
        if not tag:
            break
        tags[tag] = value
    hash_at = file_offset(data, tags[4])
    nchain = struct.unpack_from('<I', data, hash_at + 4)[0]
    strings = file_offset(data, tags[5])
    table = file_offset(data, tags[6])
    if tags[11] != 16 or table + nchain * 16 > len(data):
        raise ValueError('Invalid dynamic symbol geometry')
    found = {}
    for i in range(nchain):
        name, value, _, info, _, section = struct.unpack_from('<IIIBBH', data, table + i * 16)
        if section and info >> 4 in (1, 2):
            end = data.index(b'\0', strings + name, strings + tags[10])
            found[data[strings + name:end].decode('ascii')] = value
    return found


def file_offset(data, address):
    if data[:7] != b'\x7fELF\x01\x01\x01' or struct.unpack_from('<H', data, 18)[0] != 40:
        raise ValueError('Expected ELF32 little-endian ARM')
    phoff = struct.unpack_from('<I', data, 28)[0]
    entsize, count = struct.unpack_from('<HH', data, 42)
    for i in range(count):
        kind, offset, va, _, filesz = struct.unpack_from('<5I', data, phoff + i * entsize)
        if kind == 1 and va <= address < va + filesz:
            return offset + address - va
    raise ValueError('Symbol outside file-backed LOAD segments')


def main():
    root = Path('private/analysis')
    air = root / 'mu1476-aio-app50/files/eso/lib/libairplay.so'
    nv = root / 'mu1476-aio-app50/files/armle/lib/libnvss_video.so'
    kd = root / 'mu1476-aio-mifs50/files/lib/libKD.so'
    api = '''AirPlayCopyServerInfo AirPlayReceiverSessionSendCommand
        AES_CBCFrame_Init AES_CTR_Init AES_CTR_Update AES_CTR_Final
        AirPlay_DeriveAESKeySHA512ForScreen CFStringCreateWithCString
        CFStringGetCStringPtr CFStringGetCString CFGetTypeID CFStringGetTypeID
        CFDictionaryGetTypeID CFArrayGetTypeID CFNumberGetTypeID CFBooleanGetTypeID
        CFDataGetTypeID CFGetInt64 CFBooleanGetValue CFDataGetLength CFDataGetBytePtr
        CFDictionaryGetCount CFDictionaryGetKeysAndValues CFDictionaryGetValue
        CFDictionarySetValue CFDictionaryRemoveValue CFDictionaryGetInt64
        CFDictionarySetInt64 CFDictionaryCreateMutable CFArrayGetCount
        CFArrayGetValueAtIndex CFArrayAppendValue CFArrayCreateMutable
        CFArrayCreateMutableCopy CFRetain CFRelease kCFLDictionaryKeyCallBacksCFLTypes
        kCFLDictionaryValueCallBacksCFLTypes kCFLArrayCallBacksCFLTypes
        kCFLBooleanFalse AirPlayReceiverSessionSetSecurityInfo'''.split()
    expected = {
        'AirPlayReceiverSessionSetup': (0xe92d4ff0, 0xed2d8b02),
        'AirPlayReceiverSessionStart': (0xe92d4ff0, 0xed2d8b02),
        'AirPlayReceiverSessionTearDown': (0xe92d4ff0, 0xed2d8b02),
        'AirPlayReceiverSessionPlatformControl': (0xe92d4ff0, 0xed2d8b04),
        'AirPlayReceiverSessionControl': (0xe92d4ff0, 0xe1a06002),
    }
    syms, data = symbols(air), air.read_bytes()
    checks = {}
    for name, words in expected.items():
        if name not in syms:
            checks[name] = {'present': False}
            continue
        at = file_offset(data, syms[name])
        observed = struct.unpack_from('<2I', data, at)
        checks[name] = dict(symbol_value=hex(syms[name]), file_offset=hex(at),
                            observed=[hex(w) for w in observed],
                            expected=[hex(w) for w in words],
                            matches_audi_guard=observed == words)
    report = dict(
        scope='PCM4 MU1476 AIO reference, not the vehicle MU0809; static checks only',
        sha256={str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in (air, nv, kd)},
        receiver_api={n: n in syms for n in api},
        lifecycle_hooks=checks,
        optional_mode_parser='AirPlayReceiverSessionMakeModeStateFromDictionary' in syms,
        renderer_api={n: n in symbols(p) for p, names in (
            (nv, 'NvSSVideoOpen NvSSVideoClose NvSSVideoStreamConfigure NvSSVideoGetAttribs NvSSVideoSetAttribs NvSSVideoDecode'),
            (kd, 'kdInitializeNV kdTerminateNV')) for n in names.split()},
        limitations=['Export presence does not verify ABI or internal semantics.',
                     'Eight-byte guards do not verify trampoline relocation safety.',
                     'Security call site, HMI bytecode and physical display route remain unverified.'])
    dest = root / 'audi-pcm4-interface-comparison.json'
    dest.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
