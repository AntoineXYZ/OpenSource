#!/usr/bin/env python3
"""Read original 2016 panel PNG resources; print evidence without writing files.

The fixed registrations are in AuSceneClientRes13.dll (0x1030), AuData13.dll
(0x1140), and AuGlobal13.dll (0x28d0). They use version-1 Qt resource trees.
Bytes are preserved unchanged after Qt's optional
compression wrapper is decoded. No DLL is loaded or executed.
"""
import argparse
import base64
import hashlib
import json
import struct
import zlib
from pathlib import Path


EXTRAS = ('UpgradeModule.png', 'Troop.png', 'c_SpaceFighter.png',
          'c_SpaceFighter2.png', 'c_MotorBike.png', 'c_Suv.png', 'c_APC.png',
          'c_CommercialAircraft.png', 'c_DesignDisk.png')


def resources(data, init=0x1030):
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    count = struct.unpack_from('<H', data, pe + 6)[0]
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    sections = [struct.unpack_from('<8sIIII', data, pe + 24 + optional_size + i * 40)
                for i in range(count)]

    def offset(rva):
        for _, _, address, size, position in sections:
            if address <= rva < address + size:
                return position + rva - address
        raise ValueError('Unmapped PE RVA ' + hex(rva))

    start = offset(init)
    if data[start:start + 7] != bytes.fromhex('4883ec284c8d0d'):
        raise ValueError('Unsupported resource DLL registration')
    if data[start + 25:start + 30] != bytes.fromhex('b901000000'):
        raise ValueError('Expected Qt resource format version 1')
    payload = offset(init + 11 + struct.unpack_from('<i', data, start + 7)[0])
    names = offset(init + 18 + struct.unpack_from('<i', data, start + 14)[0])
    tree = offset(init + 25 + struct.unpack_from('<i', data, start + 21)[0])

    def walk(index, path):
        entry = tree + 14 * index
        name_offset, flags = struct.unpack_from('>IH', data, entry)
        length = struct.unpack_from('>H', data, names + name_offset)[0]
        name = data[names + name_offset + 6:names + name_offset + 6 + length * 2].decode('utf-16-be') if index else ''
        path = path + '/' + name if name else path
        if flags & 2:
            children, first = struct.unpack_from('>II', data, entry + 6)
            for child in range(first, first + children):
                yield from walk(child, path)
        else:
            relative = struct.unpack_from('>I', data, entry + 10)[0]
            size = struct.unpack_from('>I', data, payload + relative)[0]
            raw = data[payload + relative + 4:payload + relative + 4 + size]
            if len(raw) != size:
                raise ValueError('Truncated resource')
            if flags & 1:
                expected = struct.unpack_from('>I', raw)[0]
                raw = zlib.decompress(raw[4:])
                if len(raw) != expected:
                    raise ValueError('Compressed resource length mismatch')
            yield path, raw

    yield from walk(0, '')


def inspect(directory, extra=()):
    wanted = set(EXTRAS) | set(extra)
    assets = []
    sources = []
    for filename, init in (('AuSceneClientRes13.dll', 0x1030),
                           ('AuData13.dll', 0x1140), ('AuGlobal13.dll', 0x28d0)):
        data = (directory / filename).read_bytes()
        sources.append(dict(file=filename, sha256=hashlib.sha256(data).hexdigest(),
                            registration_rva=hex(init)))
        for path, raw in resources(data, init):
            name = path.rsplit('/', 1)[-1]
            if not (filename == 'AuSceneClientRes13.dll' and name.startswith('dp_')
                    and name.endswith('.png')) and name not in wanted:
                continue
            if raw[:8] != b'\x89PNG\r\n\x1a\n':
                raise ValueError('Expected original PNG: ' + path)
            width, height = struct.unpack_from('>II', raw, 16)
            assets.append(dict(name=name, resource=':' + path, source=filename,
                               bytes=len(raw), width=width, height=height,
                               sha256=hashlib.sha256(raw).hexdigest(),
                               base64=base64.b64encode(raw).decode('ascii')))
            wanted.discard(name)
    if wanted:
        raise ValueError('Missing extra resources: ' + ', '.join(sorted(wanted)))
    return dict(sources=sources, resource_format=1,
                provenance='Original PNG bytes, static Qt resource decoding; no image editing',
                assets=sorted(assets, key=lambda a: a['name']))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('client_directory', type=Path)
    parser.add_argument('--extra', action='append', default=[])
    args = parser.parse_args()
    print(json.dumps(inspect(args.client_directory, args.extra), indent=2))
