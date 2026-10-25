#!/usr/bin/env python3
"""Read-only inspection of MHI2 LZOZ MMX stage-2 ImageFS containers.

Format reference: harman-f/MHI2-MHI2Q-MH2P-Firmware-Toolkit,
docs/MHI2_FLASH_LAYOUT_AND_RECOVERY.md; QNX ImageFS directory structures.
Never executes payloads or creates image symlinks. Outputs must be new.
"""
import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import re
import struct


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source', type=Path)
    p.add_argument('--lzo-library', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    if a.output.exists():
        raise FileExistsError('Output must be a new directory')
    raw = a.source.read_bytes()
    if raw[:4] != b'LZOZ':
        raise ValueError('Expected LZOZ container')
    blocksize = struct.unpack_from('<I', raw, 4)[0]
    if blocksize != 0x200000:
        raise ValueError('Unexpected expanded block size')
    codec = ctypes.CDLL(str(a.lzo_library))
    f = codec.lzo1z_decompress_safe
    f.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p,
                  ctypes.POINTER(ctypes.c_size_t), ctypes.c_void_p]
    f.restype = ctypes.c_int
    chunks, cursor = [], 0x800
    for (size,) in struct.iter_unpack('<I', raw[8:0x200]):
        if not size:
            break
        if cursor + size > len(raw):
            raise ValueError('Truncated compressed block')
        src = ctypes.create_string_buffer(raw[cursor:cursor+size])
        out = ctypes.create_string_buffer(blocksize)
        n = ctypes.c_size_t(blocksize)
        status = f(src, size, out, ctypes.byref(n), None)
        if status:
            raise ValueError(f'LZO1Z error: {status}')
        chunks.append(out.raw[:n.value])
        cursor = (cursor + size + 511) & ~511
    image = b''.join(chunks)
    if image[:7] != b'imagefs' or len(image) % 4:
        raise ValueError('Invalid ImageFS output')
    size, directory_end, at = struct.unpack_from('<III', image, 8)
    checksum = sum(w[0] for w in struct.iter_unpack('<I', image)) & 0xffffffff
    if size != len(image) or checksum or not 0 < at < directory_end <= size:
        raise ValueError('Invalid ImageFS size, directory or checksum')
    a.output.mkdir(parents=True)
    entries = []
    while at + 24 <= directory_end:
        length, extra, ino, mode, gid, uid, mtime = struct.unpack_from('<HHIIIII', image, at)
        if not length:
            break
        if length < 24 or at+length > directory_end:
            raise ValueError('Invalid record')
        kind, name_at = mode & 0xf000, at+24
        row = dict(inode=ino, mode=mode, type=hex(kind))
        content = None
        if kind == 0x8000:
            offset, size = struct.unpack_from('<II', image, name_at)
            name_at += 8
            if offset < directory_end or offset+size > len(image):
                raise ValueError('Invalid file extent')
            content = image[offset:offset+size]
            row.update(offset=offset, size=size, sha256=hashlib.sha256(content).hexdigest())
        elif kind == 0xa000:
            off, n = struct.unpack_from('<HH', image, name_at)
            name_at += 4
            if name_at+off+n > at+length:
                raise ValueError('Invalid symlink extent')
            row['target'] = image[name_at+off:name_at+off+n].rstrip(b'\0').decode()
        elif kind != 0x4000:
            raise ValueError('Unsupported record')
        name = image[name_at:at+length].split(b'\0', 1)[0].decode()
        row['name'] = name
        parts = Path(name).parts
        if Path(name).is_absolute() or '..' in parts or '\\' in name:
            raise ValueError('Unsafe path')
        if content is not None and re.search(r'(?i)(lsd\.jxe|libKD\.|nvss|display|dmdt|carplay|\.sh$|\.cfg$|\.conf$)', name):
            dest = a.output/'files'/name
            dest.parent.mkdir(parents=True, exist_ok=True)
            with dest.open('xb') as h:
                h.write(content)
        entries.append(row)
        at += length
    report = dict(source=str(a.source), source_sha256=hashlib.sha256(raw).hexdigest(),
                  blocks=len(chunks), image_bytes=len(image), checksum_sum=checksum,
                  image_sha256=hashlib.sha256(image).hexdigest(), entries=entries)
    (a.output/'imagefs.bin').write_bytes(image)
    (a.output/'inventory.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k != 'entries'}, indent=2))
    print('Entries:', len(entries))


if __name__ == '__main__':
    main()
