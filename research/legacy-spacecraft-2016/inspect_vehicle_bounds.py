#!/usr/bin/env python3
"""Read original GD vehicle models and embedded Qt resources without loading DLLs.

Replays original float32 vertex scaling for Symmetrica (.3dsX) matrices.
The actual embedded vehicle assets use only identity matrices; other transforms
are reported instead of approximated. No inputs or extracted assets are written.
"""
import argparse
import hashlib
import json
import re
import struct
import zlib
from pathlib import Path

from inspect_detail_types import read_types


VEHICLES = (6, 7, 8, 9, 28, 54, 70, 71, 77, 82, 103, 104, 132, 133, 231)


def read_gd(data):
    offset = read_types(data)['detail_table_end']

    def number(fmt):
        nonlocal offset
        value = struct.unpack_from('>' + fmt, data, offset)[0]
        offset += struct.calcsize(fmt)
        return value

    def string():
        nonlocal offset
        count = number('I')
        if count == 0xffffffff:
            return None
        if count % 2 or count > len(data) - offset:
            raise ValueError('Invalid GD string')
        value = data[offset:offset + count].decode('utf-16-be')
        offset += count
        return value

    rooms = []
    for _ in range(number('H')):
        rooms.append(dict(room_type=number('I'), strings=[string() for _ in range(4)]))
    # Replay only the explicit stream reads of the saved original Rx body.
    source = (Path(__file__).parent / 'pseudocode' /
              'AuGlobal13.dll_1800736a0_DbCommodity_Rx.txt').read_text()
    fields = re.findall(r'(?:QDataStream::)?operator>>\(param_1,\('
                        r'(short|char|int|float|double|QString) \*\)([^;]+)\);', source)
    if len(fields) != 96:
        raise ValueError('Commodity Rx evidence changed; review parser')
    commodities = []
    for _ in range(number('H')):
        start = offset
        values = []
        for kind, expression in fields:
            value = string() if kind == 'QString' else number(
                {'short': 'H', 'char': 'B', 'int': 'I', 'float': 'f', 'double': 'd'}[kind])
            values.append((expression, value))
        record = dict(values)
        commodities.append(dict(commodity=values[0][1], stream_offset=start,
                                model=record['(this + 0xe8)'], name=record['(this + 0x118)']))
    return rooms, commodities


def resources(data):
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    count = struct.unpack_from('<H', data, pe + 6)[0]
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    section_table = pe + 24 + optional_size
    sections = [struct.unpack_from('<8sIIII', data, section_table + i * 40)
                for i in range(count)]

    def file_offset(rva):
        for _, _, address, size, offset in sections:
            if address <= rva < address + size:
                return offset + rva - address
        raise ValueError('Unmapped PE RVA ' + hex(rva))

    # Seven resource registrations, verified at AuModelRes13.dll 180001030..11b0.
    for init in range(0x1030, 0x11b1, 0x40):
        start = file_offset(init)
        if data[start:start + 7] != bytes.fromhex('4883ec284c8d0d'):
            raise ValueError('Unsupported resource DLL registration code')
        payload = file_offset(init + 11 + struct.unpack_from('<i', data, start + 7)[0])
        names = file_offset(init + 18 + struct.unpack_from('<i', data, start + 14)[0])
        tree = file_offset(init + 25 + struct.unpack_from('<i', data, start + 21)[0])

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
                if flags & 1:
                    expected = struct.unpack_from('>I', raw)[0]
                    raw = zlib.decompress(raw[4:])
                    if len(raw) != expected:
                        raise ValueError('Compressed resource length mismatch')
                yield path, raw

        yield from walk(0, '')


def float32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


def model_bounds(raw, symmetrica=False):
    objects = []
    matrix_count = 0

    def chunks(start, end, owner=None):
        nonlocal matrix_count
        while start < end:
            tag, size = struct.unpack_from('<HI', raw, start)
            if size < 6 or start + size > end:
                raise ValueError('Invalid 3DS chunk')
            content, stop = start + 6, start + size
            if tag in (0x4d4d, 0x3d3d, 0x4100):
                chunks(content, stop, owner)
            elif tag == 0x4000:
                zero = raw.index(0, content, stop)
                owner = dict(name=raw[content:zero].decode('ascii'), vertices=[])
                objects.append(owner)
                chunks(zero + 1, stop, owner)
            elif tag == 0x4110:
                count = struct.unpack_from('<H', raw, content)[0]
                if content + 2 + count * 12 > stop or owner is None:
                    raise ValueError('Invalid vertex list')
                owner['vertices'].extend(struct.unpack_from('<fff', raw, content + 2 + i * 12)
                                         for i in range(count))
            elif tag == 0x4160:
                if owner is None or content + 48 > stop:
                    raise ValueError('Invalid matrix chunk')
                matrix = struct.unpack_from('<12f', raw, content)
                # LoadMatrixChunk 1800170e0 first scales existing float vertices
                # by double 1/12 for .3dsX. Conversion back to float occurs for
                # EACH coordinate, before projection and double min/max.
                if symmetrica:
                    owner['vertices'] = [tuple(float32(value * (1.0 / 12.0)) for value in p)
                                         for p in owner['vertices']]
                if matrix != (1.0, 0.0, 0.0, 0.0, 1.0, 0.0,
                              0.0, 0.0, 1.0, 0.0, 0.0, 0.0):
                    raise ValueError('Nonidentity model matrix requires complete inverse replay')
                matrix_count += 1
            start = stop

    chunks(0, len(raw))
    included, excluded, points = [], [], []
    for obj in objects:
        name = obj['name']
        station = False
        if len(name) > 3 and name.startswith('@'):
            try:
                # Assembly 180026735..180026746 proves mid(2, 2), including
                # the length argument omitted by Ghidra's guessed signature.
                station = bool(int(name[2:4], 16) & 0xff)
            except ValueError:
                pass
        weapon = len(name) > 1 and name.startswith('!')
        if not obj['vertices'] or name.startswith('-') or station or weapon:
            excluded.append(name)
        else:
            included.append(name)
            points.extend(obj['vertices'])
    if not points:
        raise ValueError('No contributing model vertices')
    minimum = [min(point[axis] for point in points) for axis in range(3)]
    maximum = [max(point[axis] for point in points) for axis in range(3)]
    half = [(maximum[axis] - minimum[axis]) / 2 for axis in range(2)]
    return dict(asset_sha256=hashlib.sha256(raw).hexdigest(), asset_bytes=len(raw),
                minimum=minimum, maximum=maximum, half_xy=half,
                half_xy_float32=[float32(value) for value in half],
                identity_matrix_chunks=matrix_count,
                symmetrica_vertex_scale=(1.0 / 12.0) if symmetrica else 1.0,
                included_objects=included, excluded_objects=excluded)


def inspect(directory):
    gd = (directory / 'GD').read_bytes()
    dll = (directory / 'AuModelRes13.dll').read_bytes()
    rooms, commodities = read_gd(gd)
    models = dict(resources(dll))
    missing_names = {entry['model'].rsplit('/', 1)[-1] for entry in commodities
                     if entry['commodity'] in VEHICLES
                     and '/' + entry['model'].lstrip(':/') not in models}
    # Qt resource-tree names are uncompressed UTF-16BE. Also check both common
    # code-string encodings in every original DLL/EXE before applying the old
    # CreateGeometry-null fallback. A discovered name must be investigated.
    absent_audit = []
    for path in sorted(directory.iterdir()):
        if path.suffix.lower() not in ('.dll', '.exe', '.rcc'):
            continue
        image = path.read_bytes()
        hits = [name for name in sorted(missing_names)
                if any(name.encode(encoding) in image
                       for encoding in ('ascii', 'utf-16-be', 'utf-16-le'))]
        if hits:
            raise ValueError(f'Unparsed resource-name evidence in {path.name}: {hits}')
        absent_audit.append(dict(file=path.name, sha256=hashlib.sha256(image).hexdigest()))
    vehicles = []
    for entry in commodities:
        if entry['commodity'] not in VEHICLES:
            continue
        record = dict(entry)
        path = '/' + entry['model'].lstrip(':/')
        try:
            if path not in models:
                # Load(QString) fails QFile::open, CreateGeometry returns null.
                # IsValid initializes both bounds to zero before that call.
                record.update(minimum=[0.0] * 3, maximum=[0.0] * 3,
                              half_xy=[0.0, 0.0], half_xy_float32=[0.0, 0.0],
                              original_missing_model_fallback=True,
                              resource_name_absent=True)
            else:
                record.update(model_bounds(models[path], entry['model'].endswith('.3dsX')))
        except (KeyError, ValueError) as error:
            record['unsupported'] = str(error)
        vehicles.append(record)
    return dict(gd_sha256=hashlib.sha256(gd).hexdigest(),
                model_dll_sha256=hashlib.sha256(dll).hexdigest(),
                missing_resource_names=sorted(missing_names),
                missing_resource_binary_audit=absent_audit,
                room_types=rooms, vehicles=vehicles,
                provenance='Static replay of original 2016 GD and embedded model resources; no DLL execution',
                supported_transform_domain='Original eleven present vehicle assets: no matrix or identity matrix only; other transforms rejected')


def self_test():
    def chunk(tag, content):
        return struct.pack('<HI', tag, len(content) + 6) + content

    identity = struct.pack('<12f', 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0)
    vertices = struct.pack('<H6f', 2, -1.1, -2.3, 3.7, 9.5, 12.5, 18.1)

    def obj(name, vertex_data=vertices, matrix=identity, matrix_first=False):
        body = chunk(0x4110, vertex_data)
        if matrix is not None:
            body = chunk(0x4160, matrix) + body if matrix_first else body + chunk(0x4160, matrix)
        return chunk(0x4000, name.encode('ascii') + b'\0' + chunk(0x4100, body))

    def model(*objects):
        return chunk(0x4d4d, chunk(0x3d3d, b''.join(objects)))

    raw_points = list(struct.iter_unpack('<fff', vertices[2:]))
    regular = model_bounds(model(obj('Hull')), False)
    scaled = model_bounds(model(obj('Hull')), True)
    assert regular['minimum'] == list(raw_points[0])
    assert regular['maximum'] == list(raw_points[1])
    expected = [tuple(float32(v * (1.0 / 12.0)) for v in p) for p in raw_points]
    assert scaled['minimum'] == list(expected[0])
    assert scaled['maximum'] == list(expected[1])
    assert scaled['identity_matrix_chunks'] == 1
    assert any(expected[row][axis] != raw_points[row][axis] / 12.0
               for row in range(2) for axis in range(3))
    # Scaling is a matrix-chunk event, never an unconditional extension-wide
    # post-process: a matrix preceding the vertex list scales an empty array.
    assert model_bounds(model(obj('Hull', matrix_first=True)), True)['maximum'] == regular['maximum']
    assert model_bounds(model(obj('Hull', matrix=None)), True)['maximum'] == regular['maximum']
    far = struct.pack('<H3f', 1, 10000, 10000, 10000)
    excluded = model_bounds(model(obj('Hull'), obj('@040longsuffix', far), obj('!0', far), obj('-Hidden', far)))
    assert excluded['maximum'] == regular['maximum']
    assert excluded['excluded_objects'] == ['@040longsuffix', '!0', '-Hidden']
    assert model_bounds(model(obj('Hull'), obj('@000', far)))['maximum'] == [10000] * 3
    rotated = struct.pack('<12f', 0, 1, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0)
    try:
        model_bounds(model(obj('Hull', matrix=rotated)))
    except ValueError:
        pass
    else:
        raise AssertionError('Unsupported matrix was not rejected')
    print('PASS: native-float scaling, matrix order, missing matrix, object exclusions, exact station substring, unsupported-transform guard')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('original_client', type=Path, nargs='?')
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test()
    elif args.original_client is not None:
        print(json.dumps(inspect(args.original_client), indent=2))
    else:
        parser.error('provide original_client or --self-test')
