#!/usr/bin/env python3
"""Read-only inventory of little-endian, LZO-compressed QNX IFS images.

Format references (not vendor firmware):
https://github.com/lclevy/dumpifs/blob/main/dumpifs.py
https://github.com/askac/dumpifs/tree/master/sys

Uses an explicitly selected local liblzo2; never runs image contents.
Writes decompressed bytes and metadata only to the supplied output directory.
"""

import argparse
import collections
import ctypes
import hashlib
import json
from pathlib import Path
import struct


def inspect(source, library, destination):
    raw = source.read_bytes()
    if raw[:4] != bytes.fromhex("eb7eff00"):
        raise ValueError("Expected a QNX startup header at offset zero")
    if raw[6] & 2 or ((raw[6] >> 2) & 7) != 2:
        raise ValueError("Only little-endian LZO images are supported")
    start, stored = struct.unpack_from("<II", raw, 0x20)
    expected = struct.unpack_from("<I", raw, 0x2C)[0]
    if stored != len(raw) or not 0 < start < stored or expected > 512 * 1024**2:
        raise ValueError("Inconsistent or oversized startup header")
    codec = ctypes.CDLL(str(library))
    decompress = codec.lzo1x_decompress_safe
    decompress.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_void_p,
                           ctypes.POINTER(ctypes.c_size_t), ctypes.c_void_p]
    decompress.restype = ctypes.c_int
    cursor, blocks, chunks, total = start, 0, [], 0
    while True:
        count = struct.unpack_from(">H", raw, cursor)[0]
        cursor += 2
        if not count:
            break
        if cursor + count > stored:
            raise ValueError("Truncated LZO block")
        src = ctypes.create_string_buffer(raw[cursor:cursor + count])
        out = ctypes.create_string_buffer(65536)
        out_size = ctypes.c_size_t(len(out))
        status = decompress(src, count, out, ctypes.byref(out_size), None)
        if status:
            raise ValueError(f"LZO failure at {cursor:#x}: {status}")
        chunks.append(out.raw[:out_size.value])
        total += out_size.value
        if total > expected:
            raise ValueError("Decompressed image exceeds advertised size")
        cursor += count
        blocks += 1
    image = b"".join(chunks)
    if len(image) != expected or image[:7] != b"imagefs" or len(image) % 4:
        raise ValueError("Invalid image size or ImageFS signature")
    checksum = sum(word[0] for word in struct.iter_unpack("<I", image)) & 0xFFFFFFFF
    if checksum:
        raise ValueError("ImageFS additive checksum does not balance")
    image_size, directory_end, at = struct.unpack_from("<III", image, 8)
    if image_size != len(image) or not 0 < at < directory_end <= len(image):
        raise ValueError("Invalid ImageFS directory boundaries")
    entries = []
    while at + 24 <= directory_end:
        length, extra, inode, mode, gid, uid, mtime = struct.unpack_from("<HHIIIII", image, at)
        if not length:
            break
        if length < 24 or at + length > directory_end:
            raise ValueError("Invalid directory record")
        kind, name_at = mode & 0xF000, at + 24
        row = dict(type=hex(kind), mode=oct(mode), inode=inode, uid=uid, gid=gid, mtime=mtime)
        if kind == 0x8000:
            offset, size = struct.unpack_from("<II", image, name_at)
            name_at += 8
            if offset < directory_end or offset + size > len(image):
                raise ValueError("Invalid file extent")
            content = image[offset:offset + size]
            row.update(offset=offset, size=size, sha256=hashlib.sha256(content).hexdigest())
            if content[:4] == b"\x7fELF":
                row.update(elf_class=content[4], elf_endian=content[5],
                           elf_machine=struct.unpack_from("<H", content, 18)[0])
        elif kind == 0xA000:
            target_offset, target_size = struct.unpack_from("<HH", image, name_at)
            name_at += 4
            target_at = name_at + target_offset
            if target_at + target_size > at + length:
                raise ValueError("Invalid symlink extent")
            row["target"] = image[target_at:target_at + target_size].rstrip(b"\0").decode()
        elif kind != 0x4000:
            raise ValueError(f"Unsupported entry type {kind:#x}")
        row["name"] = image[name_at:at + length].split(b"\0", 1)[0].decode()
        entries.append(row)
        at += length
    report = dict(source_name=source.name, source_bytes=len(raw),
                  source_sha256=hashlib.sha256(raw).hexdigest(), compression="LZO",
                  blocks=blocks, compressed_end=cursor, image_bytes=len(image),
                  image_sha256=hashlib.sha256(image).hexdigest(),
                  image_checksum_sum=checksum, entries=entries)
    destination.mkdir(parents=True, exist_ok=True)
    for filename in ("imagefs.bin", "inventory.json"):
        if (destination / filename).exists():
            raise FileExistsError(f"Refusing to overwrite {destination / filename}")
    (destination / "imagefs.bin").write_bytes(image)
    (destination / "inventory.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(dict(source_bytes=len(raw), image_bytes=len(image), blocks=blocks,
                          entries=len(entries), checksum_sum=checksum,
                          entry_types=dict(collections.Counter(e["type"] for e in entries)),
                          elf_machines=dict(collections.Counter(e["elf_machine"] for e in entries
                                                                if "elf_machine" in e))), indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--lzo-library", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    inspect(args.source, args.lzo_library, args.output)
