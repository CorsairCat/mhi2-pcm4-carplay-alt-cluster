#!/usr/bin/env python3
"""Read-only inventory and selective extraction of standard little-endian QNX6.

Format references: Linux include/linux/qnx6_fs.h and fs/qnx6/{inode,dir}.c.
No mounting, device access, symlink creation, or execution of image contents.
This is a bounded research reader, not a filesystem repair or image writer.
"""
import argparse
import hashlib
import json
import mmap
from pathlib import Path
import re
import stat
import struct


def crc32_be(data):
    crc = 0
    for byte in data:
        crc ^= byte << 24
        for _ in range(8):
            crc = ((crc << 1) ^ (0x04C11DB7 if crc & 0x80000000 else 0)) & 0xffffffff
    return crc


class QNX6:
    def __init__(self, image):
        self.handle = image.open('rb')
        self.data = mmap.mmap(self.handle.fileno(), 0, access=mmap.ACCESS_READ)
        self.sb_at = next((o for o in (0x2000, 0) if self.data[o:o+4] == b'\x22\x11\x19\x68'), None)
        if self.sb_at is None:
            raise ValueError('No standard little-endian QNX6 superblock')
        first = self.superblock(self.sb_at)
        self.bs = first['block_size']
        if self.bs < 512 or self.bs > 65536 or self.bs & (self.bs - 1):
            raise ValueError('Unsupported block size')
        self.base = self.sb_at + 0x1000
        second_at = self.base + first['blocks'] * self.bs
        second = self.superblock(second_at)
        self.superblocks = [first, second]
        if first['block_size'] != second['block_size'] or first['blocks'] != second['blocks']:
            raise ValueError('Inconsistent superblocks')
        self.sb = max(self.superblocks, key=lambda s: s['serial'])
        sb = self.data[self.sb['offset']:self.sb['offset']+512]
        self.inodes = self.root(sb, 72)
        self.longnames = self.root(sb, 232)
        self.inode_data = self.content(self.inodes)

    def superblock(self, offset):
        b = self.data[offset:offset+512]
        if len(b) != 512 or b[:4] != b'\x22\x11\x19\x68':
            raise ValueError('Missing superblock')
        checksum = struct.unpack_from('<I', b, 4)[0]
        if checksum != crc32_be(b[8:]):
            raise ValueError('Superblock checksum mismatch')
        return dict(offset=offset, serial=struct.unpack_from('<Q', b, 8)[0],
                    block_size=struct.unpack_from('<I', b, 48)[0],
                    blocks=struct.unpack_from('<I', b, 60)[0], checksum_valid=True)

    @staticmethod
    def root(data, at):
        return dict(size=struct.unpack_from('<Q', data, at)[0],
                    pointers=struct.unpack_from('<16I', data, at+8), levels=data[at+72])

    def block(self, pointer):
        if not 0 <= pointer < self.sb['blocks']:
            raise ValueError('Out-of-range block pointer')
        at = self.base + pointer * self.bs
        b = self.data[at:at+self.bs]
        if len(b) != self.bs:
            raise ValueError('Truncated block')
        return b

    def content(self, node):
        size, depth = node['size'], node['levels']
        if depth > 5 or size > len(self.data):
            raise ValueError('Unsupported node geometry')
        remaining, parts = size, []

        def visit(pointer, level):
            nonlocal remaining
            if not remaining:
                return
            if pointer == 0xffffffff:
                raise ValueError('Unused pointer within file extent')
            b = self.block(pointer)
            if level:
                for (p,) in struct.iter_unpack('<I', b):
                    if not remaining:
                        break
                    visit(p, level-1)
            else:
                count = min(remaining, self.bs)
                parts.append(b[:count])
                remaining -= count
        for p in node['pointers']:
            if not remaining:
                break
            visit(p, depth)
        if remaining:
            raise ValueError('Incomplete file pointer tree')
        return b''.join(parts)

    def inode(self, number):
        at = (number - 1) * 128
        if number < 1 or at + 128 > len(self.inode_data):
            raise ValueError('Invalid inode')
        b = self.inode_data[at:at+128]
        return dict(inode=number, size=struct.unpack_from('<Q', b)[0],
                    mode=struct.unpack_from('<H', b, 32)[0],
                    pointers=struct.unpack_from('<16I', b, 36), levels=b[100])

    def walk(self):
        longnames = self.content(self.longnames)

        def descend(number, path, ancestors):
            node = self.inode(number)
            yield path, node
            if not stat.S_ISDIR(node['mode']):
                return
            if number in ancestors or len(ancestors) > 64:
                raise ValueError('Directory cycle or excessive depth')
            b = self.content(node)
            if len(b) % 32:
                raise ValueError('Unaligned directory')
            for at in range(0, len(b), 32):
                ino = struct.unpack_from('<I', b, at)[0]
                length = b[at+4]
                if not ino or not length:
                    continue
                if length <= 27:
                    name = b[at+5:at+5+length]
                elif length == 255:
                    off = struct.unpack_from('<I', b, at+8)[0] * self.bs
                    n = struct.unpack_from('<H', longnames, off)[0]
                    if not 0 < n <= 510 or off+2+n > len(longnames):
                        raise ValueError('Invalid long name')
                    name = longnames[off+2:off+2+n]
                else:
                    raise ValueError('Invalid directory name length')
                name = name.decode('utf-8')
                if name in ('.', '..'):
                    continue
                if '/' in name or '\\' in name or '\0' in name:
                    raise ValueError('Unsafe name')
                yield from descend(ino, path+'/'+name if path else name, ancestors | {number})
        yield from descend(1, '', set())


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('image', type=Path)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--select', default=r'(?i)(airplay|dio_manager|lsd\.jxe|libnvss|libKD\.|display|dmdt|carplay|kombi|komo|traffic|navigation|startup|\.sh$|\.cfg$|\.conf$)')
    args = p.parse_args()
    if args.output.exists():
        raise FileExistsError('Output must be a new directory')
    q = QNX6(args.image)
    args.output.mkdir(parents=True)
    entries = []
    for path, node in q.walk():
        row = {k: node[k] for k in ('inode', 'size', 'mode')}
        row['path'] = path
        if stat.S_ISLNK(node['mode']):
            row['target'] = q.content(node).decode('utf-8')
        elif stat.S_ISREG(node['mode']) and re.search(args.select, path):
            content = q.content(node)
            row['sha256'] = hashlib.sha256(content).hexdigest()
            dest = args.output / 'files' / path
            dest.parent.mkdir(parents=True, exist_ok=True)
            with dest.open('xb') as f:
                f.write(content)
            if content[:4] == b'\x7fELF':
                row.update(elf_class=content[4], elf_endian=content[5],
                           elf_machine=struct.unpack_from('<H', content, 18)[0])
        entries.append(row)
    report = dict(source=str(args.image), superblocks=q.superblocks,
                  selected_superblock=q.sb, entries=entries)
    (args.output / 'inventory.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(dict(superblocks=q.superblocks, entries=len(entries),
                          extracted=sum('sha256' in e for e in entries)), indent=2))


if __name__ == '__main__':
    main()
