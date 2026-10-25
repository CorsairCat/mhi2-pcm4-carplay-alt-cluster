#!/usr/bin/env python3
"""Bounded, read-only metadata and bytecode inspection for the MU1476 J9 image.

Not a JAR converter, decompiler or patcher. Rejects native methods and unknown
opcodes instead of silently guessing method/instruction boundaries. Format
cross-reference: https://github.com/luka-dev/jxe2jar (src/jxe.py, bytecode.py).
This reader is original project code; no external converter is installed/run.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = '1eccb000da3ef57e35dec0d59e4b0be5df375b20b41457fcd53f9bca75307a88'
DEFAULT = r'(DisplayManagerMIB2PorscheHigh|DisplayManagerMIB2High|/fwhmi/DisplayManager$|/context/StatePorsche|/instances/MapKombi$|/instances/MapConfigFactory$|/cluster/CombiBAPListener$|/map/MapConfig$|/ATIPHMIActivatorEvo$|/LayoutProviderPAGMainKombi$|/GUIKombiFPKPorsche$|/MapKombiFPKPorsche$|^org/dsi/ifc/map/Rect$|/CtxKombiInit$)'


class ROM:
    def __init__(self, data):
        self.data = data
        # This image's declared ROM extent excludes its 48-byte image header.
        if data[:4] != b'J99J' or self.u32(8) + 48 != len(data):
            raise ValueError('Unsupported ROM header or size')
        self.count = self.u32(12)
        if self.count > 100000:
            raise ValueError('Excessive class count')
        toc = self.relative(20)
        self.extent(toc, self.count * 8)
        self.classes = {}
        for at in range(toc, toc + self.count * 8, 8):
            name, ptr = self.string_ref(at), self.relative(at + 4)
            self.extent(ptr, 104)
            size = self.u32(ptr)
            if size < 104:
                raise ValueError('Short class')
            self.extent(ptr, size)
            if name != self.string_ref(ptr + 8) or name in self.classes:
                raise ValueError('Class name mismatch or duplicate')
            self.classes[name] = ptr

    def extent(self, at, size):
        if at < 0 or size < 0 or at > len(self.data) - size:
            raise ValueError('ROM extent out of bounds')

    def number(self, fmt, at):
        self.extent(at, struct.calcsize(fmt))
        return struct.unpack_from(fmt, self.data, at)[0]

    def u32(self, at): return self.number('<I', at)
    def relative(self, at): return at + self.number('<i', at)

    def string_ref(self, at):
        if self.u32(at) == 0:
            return ''
        ptr = self.relative(at)
        size = self.number('<H', ptr)
        self.extent(ptr + 2, size)
        # Names/descriptors under investigation are ASCII. Preserve other data
        # without interpreting Modified UTF-8 as ordinary Unicode.
        return self.data[ptr + 2:ptr + 2 + size].decode('utf-8', errors='backslashreplace')

    def constants(self, ptr):
        base, count = ptr + 104, self.u32(ptr + 56)
        if count > 65535:
            raise ValueError('Excessive constant count')
        self.extent(base, count * 8)
        if base + count * 8 > ptr + self.u32(ptr):
            raise ValueError('Constant pool escapes class extent')
        found = []
        for index in range(count):
            at = base + 8 * index
            value, tag = self.u32(at), self.u32(at + 4)
            item = dict(index=index, raw=[value, tag])
            if tag == 0:
                item.update(kind='word', value=value)
            elif tag in (1, 2):
                item.update(kind='string' if tag == 1 else 'class', value=self.string_ref(at))
            else:
                # Other slots may be 64-bit values. Do not classify them as
                # member references unless the entire pointer chain validates.
                try:
                    if value >= count or self.u32(base + value * 8 + 4) != 2:
                        raise ValueError('Not a class slot')
                    member = self.relative(at + 4)
                    name, signature = self.string_ref(member), self.string_ref(member + 4)
                    if not name or not signature or signature[0] not in '([LBCDFIJSZ':
                        raise ValueError('Invalid member name/descriptor')
                    item.update(kind='member', owner=self.string_ref(base + value * 8),
                                name=name, descriptor=signature)
                except ValueError:
                    item.update(kind='uninterpreted')
            found.append(item)
        return found

    def inspect_class(self, name):
        ptr = self.classes[name]
        cp = self.constants(ptr)
        at, count = self.relative(ptr + 32), self.u32(ptr + 28)
        if count > 10000:
            raise ValueError('Excessive method count')
        methods = []
        for _ in range(count):
            self.extent(at, 20)
            method_name, descriptor = self.string_ref(at), self.string_ref(at + 4)
            if not method_name or not descriptor.startswith('('):
                raise ValueError('Invalid method header')
            flags = self.u32(at + 8)
            if flags & 0x100:
                raise ValueError(f'Native method layout unsupported: {name}.{method_name}')
            words = self.number('<H', at + 14)
            if flags & 0x8000:
                words |= self.data[at + 16] << 16
            size, body_at = words * 4, at + 20
            self.extent(body_at, size)
            end = body_at + size
            if flags & 0x02000000:
                end += 4
            if flags & 0x20000:
                catches, throws = self.number('<H', end), self.number('<H', end + 2)
                end += 4 + catches * 16 + throws * 4
            self.extent(at, end - at)
            if not (ptr <= at < end <= ptr + self.u32(ptr)):
                raise ValueError('Method escapes class extent')
            body = self.data[body_at:body_at + size]
            method = dict(name=method_name, descriptor=descriptor, flags=hex(flags),
                          offset=hex(at), bytecode_offset=hex(body_at),
                          bytecode_bytes=size, bytecode_sha256=hashlib.sha256(body).hexdigest())
            try:
                method['instructions'] = instructions(body, cp)
            except ValueError as exc:
                method['decode_error'] = str(exc)
            methods.append(method)
            at = end
        return dict(name=name, superclass=self.string_ref(ptr + 12),
                    offset=hex(ptr), size=self.u32(ptr), constants=cp, methods=methods)


def instructions(body, cp):
    """Decode instruction boundaries and CP operands; no Java execution."""
    two = {0x10, 0x12, *range(0x15, 0x1a), *range(0x36, 0x3b), 0xa9, 0xbc}
    three = {0x11, 0x13, 0x14, 0x84, *range(0x99, 0xa9), *range(0xb2, 0xba),
             0xbb, 0xbd, 0xc0, 0xc1, 0xc6, 0xc7, *range(0xcb, 0xd5), 0xf9}
    simple = {*range(0x10), *range(0x1a, 0x36), *range(0x3b, 0x84),
              *range(0x85, 0x99), *range(0xac, 0xb2), 0xbe, 0xbf, 0xc2, 0xc3,
              0xd7, 0xe4, 0xe5, *range(0xf3, 0xf9), 0xfa}
    refs = {0x12, 0x13, 0x14, *range(0xb2, 0xba), 0xbb, 0xbd, 0xc0, 0xc1, 0xc5, 0xe7, 0xf9}
    names = {0x10:'bipush', 0x11:'sipush', 0x12:'ldc', 0x13:'ldc_w', 0x59:'dup',
             0x4f:'iastore', 0x53:'aastore', 0xac:'return0', 0xad:'return1',
             0xb2:'getstatic', 0xb3:'putstatic', 0xb4:'getfield', 0xb5:'putfield',
             0xb6:'invokevirtual', 0xb7:'invokespecial', 0xb8:'invokestatic',
             0xb9:'invokeinterface', 0xbb:'new', 0xbc:'newarray',
             0xbd:'anewarray', 0xd7:'aload0getfield-prefix', 0xe7:'invokeinterface2'}
    result, at, branch_targets = [], 0, []
    while at < len(body):
        op = body[at]
        size = 2 if op in two else 3 if op in three else 1 if op in simple else None
        if op == 0xc5: size = 4
        if op in (0xc8, 0xd5, 0xe7): size = 5
        if op in (0xaa, 0xab):
            start = (at + 4) & ~3
            minimum = 12 if op == 0xaa else 8
            if start + minimum > len(body): raise ValueError('Truncated switch header')
            if op == 0xaa:
                lo, hi = struct.unpack_from('<ii', body, start + 4)
                count, width = hi - lo + 1, 4
            else:
                count, width = struct.unpack_from('<i', body, start + 4)[0], 8
            if count < 0 or count > len(body): raise ValueError('Invalid switch count')
            size = start - at + minimum + count * width
            offsets = [struct.unpack_from('<i', body, start)[0]]
            offsets += [struct.unpack_from('<i', body, start + minimum + i * width + (4 if op == 0xab else 0))[0] for i in range(count)] if at + size <= len(body) else []
            branch_targets.extend(at + rel for rel in offsets)
        if size is None or at + size > len(body):
            raise ValueError(f'Unknown/truncated opcode {op:#x} at {at:#x}')
        row = dict(pc=at, opcode=hex(op), operation=names.get(op, f'op_{op:02x}'), raw=body[at:at+size].hex())
        if op in (0xaa, 0xab):
            row['default_target'] = at + offsets[0]
            keys = list(range(lo, hi + 1)) if op == 0xaa else [struct.unpack_from('<i', body, start + 8 + i * 8)[0] for i in range(count)]
            row['cases'] = {str(key): at + rel for key, rel in zip(keys, offsets[1:])}
        if op in refs:
            index = body[at+1] if op == 0x12 else struct.unpack_from('<H', body, at + (3 if op == 0xe7 else 1))[0]
            if index >= len(cp): raise ValueError(f'CP index {index} out of range at {at:#x}')
            row['constant'] = cp[index]
        if 0x02 <= op <= 0x08: row['integer'] = op - 3
        if op == 0x10: row['integer'] = struct.unpack_from('<b', body, at+1)[0]
        if op == 0x11: row['integer'] = struct.unpack_from('<h', body, at+1)[0]
        if 0x99 <= op <= 0xa8 or op in (0xc6, 0xc7, 0xc8):
            row['target'] = at + struct.unpack_from('<i' if op == 0xc8 else '<h', body, at+1)[0]
            branch_targets.append(row['target'])
        result.append(row)
        at += size
    starts = {row['pc'] for row in result}
    if any(target not in starts for target in branch_targets):
        raise ValueError('Branch does not target an instruction boundary')
    return result


def literal_contexts(cls):
    """Recognize literal DisplayContext(int, int[]) constructors, not execute HMI.

    Refuses a new pattern instead of treating unknown instructions as constants.
    Constructor sites on conditional paths remain declarations, not active state.
    """
    result = []
    method = next(m for m in cls['methods'] if m['name'] == 'defineContexts')
    rows = method['instructions']
    for start, row in enumerate(rows):
        if row['operation'] != 'new' or row.get('constant', {}).get('value') != 'org/dsi/ifc/displaymanagement/DisplayContext':
            continue
        at = start + 1
        if rows[at]['operation'] != 'dup': raise ValueError('Unexpected constructor pattern')
        context, count = rows[at+1]['integer'], rows[at+2]['integer']
        at += 3
        if rows[at]['raw'] != 'bc0a' or not 0 <= count <= 256:
            raise ValueError('Unsupported displayable array')
        values, assigned = [0] * count, set()
        at += 1
        while rows[at]['operation'] == 'dup':
            index, value = rows[at+1]['integer'], rows[at+2]['integer']
            if rows[at+3]['operation'] != 'iastore' or not 0 <= index < count or index in assigned:
                raise ValueError('Unexpected array store')
            values[index] = value
            assigned.add(index)
            at += 4
        target = rows[at].get('constant', {})
        if (rows[at]['operation'] != 'invokespecial' or target.get('name') != '<init>' or
            target.get('owner') != 'org/dsi/ifc/displaymanagement/DisplayContext' or
            target.get('descriptor') != '(I[I)V' or len(assigned) != count):
            raise ValueError('Unverified context constructor')
        result.append(dict(context=context, displayables=values, method_pc=row['pc']))
    return result


def porsche_layout_geometry(cls):
    """Recognize the exact literal-return patterns reviewed in this provider."""
    methods = {m['name']: m for m in cls['methods']}
    dimensions = {}
    for method_name, key in (('getScreenWidth', 'width'), ('getScreenHeight', 'height')):
        rows = methods[method_name]['instructions']
        if len(rows) != 2 or 'integer' not in rows[0] or rows[1]['opcode'] != '0xad':
            raise ValueError('Unsupported screen dimension method')
        dimensions[key] = rows[0]['integer']
    method = methods['getVARect']
    rows = method['instructions']
    if (rows[0]['operation'] != 'new' or rows[0]['constant'].get('value') != 'org/dsi/ifc/map/Rect' or
        rows[1]['operation'] != 'dup' or rows[6]['operation'] != 'invokespecial' or
        rows[6]['constant'].get('owner') != 'org/dsi/ifc/map/Rect' or
        rows[6]['constant'].get('name') != '<init>' or rows[6]['constant'].get('descriptor') != '(IIII)V' or
        rows[7]['opcode'] != '0xad'):
        raise ValueError('Unsupported navigation visible-area constructor')
    rectangle = dict(zip(('x', 'y', 'width', 'height'), (row['integer'] for row in rows[2:6])))
    return dict(provider=cls['name'], screen=dimensions, navigation_visible_area=rectangle,
                rectangle_method_rom_offset=method['offset'],
                scope='Static Porsche FPK layout branch; panel mask and CarPlay safe-area interpretation unverified')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jxe', type=Path, default=ROOT / 'private/analysis/mu1476-aio-mifs50/files/ifs/lsd.jxe')
    parser.add_argument('--select', default=DEFAULT, help='Regex over class names')
    parser.add_argument('--output', type=Path, default=ROOT / 'private/analysis/mu1476-display-port/hmi-methods.json')
    args = parser.parse_args()
    digest = hashlib.sha256(args.jxe.read_bytes()).hexdigest()
    if digest != EXPECTED:
        raise SystemExit('Unsupported JXE hash; reader scoped to reviewed MU1476 reference')
    with zipfile.ZipFile(args.jxe) as archive:
        member = archive.getinfo('rom.classes')
        if member.file_size > 128 * 1024 * 1024:
            raise ValueError('ROM size limit exceeded')
        data = archive.read(member)
    rom = ROM(data)
    selected, errors = [], {}
    for name in rom.classes:
        if not re.search(args.select, name): continue
        try: selected.append(rom.inspect_class(name))
        except ValueError as exc: errors[name] = str(exc)
    report = dict(jxe_sha256=digest, rom_sha256=hashlib.sha256(data).hexdigest(),
                  class_count=rom.count, validated_class_directory=True,
                  selection=args.select, classes=selected, errors=errors,
                  scope='Metadata and J9 instruction inspection only; no full decompilation, rewriting or execution')
    for cls in selected:
        if cls['name'].endswith('/LayoutProviderPAGMainKombi'):
            report['porsche_layout_geometry'] = porsche_layout_geometry(cls)
        if cls['name'].endswith('/DisplayManagerMIB2PorscheHigh'):
            report['porsche_declared_contexts'] = literal_contexts(cls)
            profile = json.loads((ROOT / 'profiles/pcm4-mu1476-display.json').read_text())['ownership']
            method = next(m for m in cls['methods'] if m['name'] == 'switchContext' and m['descriptor'] == profile['switch_method_descriptor'])
            site = next(i for i in method['instructions'] if i['pc'] == profile['hook_before_super_call_pc'])
            report['profile_seam_matches'] = (
                cls['name'] == profile['hmi_class'] and method['offset'] == profile['switch_method_rom_offset'] and
                site['operation'] == 'invokespecial' and
                site.get('constant', {}).get('owner') == 'de/audi/tghu/fwhmi/DisplayManager' and
                site['constant'].get('name') == 'switchContext' and
                site['constant'].get('descriptor') == profile['switch_method_descriptor'])
            if not report['profile_seam_matches']:
                raise ValueError('Profile HMI seam does not match inspected bytecode')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    methods = [m for c in selected for m in c['methods']]
    decoded = sum('instructions' in m for m in methods)
    print(f'{rom.count} directory entries validated; {len(selected)} classes inspected; {decoded}/{len(methods)} method bodies decoded; {len(errors)} class errors')
    print(args.output)


if __name__ == '__main__':
    main()
