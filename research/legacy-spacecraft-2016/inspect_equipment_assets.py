#!/usr/bin/env python3
"""Compare current equipment assets with original Qt resources; writes no files."""
import argparse
import base64
import hashlib
import json
import re
import struct
import zlib
from pathlib import Path
from inspect_vehicle_bounds import resources
from inspect_panel_icons import resources as qt_resources


def rgba(raw):
    width, height, depth, kind, _, _, interlace = struct.unpack_from('>IIBBBBB', raw, 16)
    if depth != 8 or kind not in (2, 6) or interlace:
        raise ValueError('Unsupported PNG layout')
    channels = 3 if kind == 2 else 4
    compressed = bytearray()
    offset = 8
    while offset < len(raw):
        size = struct.unpack_from('>I', raw, offset)[0]
        if raw[offset + 4:offset + 8] == b'IDAT':
            compressed.extend(raw[offset + 8:offset + 8 + size])
        offset += size + 12
    data = zlib.decompress(compressed)
    stride = width * channels
    previous = bytearray(stride)
    result = bytearray()
    for y in range(height):
        start = y * (stride + 1)
        method = data[start]
        row = bytearray(data[start + 1:start + 1 + stride])
        for x in range(stride):
            left = row[x - channels] if x >= channels else 0
            up = previous[x]
            upper_left = previous[x - channels] if x >= channels else 0
            if method == 1:
                predict = left
            elif method == 2:
                predict = up
            elif method == 3:
                predict = (left + up) // 2
            elif method == 4:
                estimate = left + up - upper_left
                distances = (abs(estimate - left), abs(estimate - up), abs(estimate - upper_left))
                predict = (left, up, upper_left)[distances.index(min(distances))]
            elif method == 0:
                predict = 0
            else:
                raise ValueError('Invalid PNG filter')
            row[x] = (row[x] + predict) & 255
        previous = row
        if channels == 4:
            result.extend(row)
        else:
            for x in range(0, len(row), 3):
                result.extend(row[x:x + 3])
                result.append(255)
    return width, height, bytes(result)


def texture_names(raw):
    names = []
    def chunks(start, end):
        while start + 6 <= end:
            tag, size = struct.unpack_from('<HI', raw, start)
            if size < 6 or start + size > end:
                raise ValueError('Invalid model chunk')
            if tag in (0x4d4d, 0x3d3d, 0xafff, 0xa200):
                chunks(start + 6, start + size)
            elif tag == 0xa300:
                names.append(raw[start + 6:start + size].split(b'\0')[0].decode())
            start += size
    chunks(0, len(raw))
    return names


def inspect(directory, repo):
    original = dict(resources((directory / 'AuModelRes13.dll').read_bytes()))
    atom = (directory / 'AuAtomRes13.dll').read_bytes()
    for init in range(0x1030, 0x2000, 0x40):
        try:
            original.update(qt_resources(atom, init))
        except ValueError:
            pass
    source = (repo / 'src/lib/designer/legacy/aulegacyshipvehicles.cpp').read_text()
    names = set(re.findall(r'"(?:\:/data/)?(m[^"/]+\.3ds[NX])"', source))
    for name in list(names):
        names.update(texture_names(original['/data/' + name]))
    names.add('mConsoleTex.png')
    names.add('txSpacecraft0Lit.png')
    names.update(re.findall(r'"(?:\:/data/|legacy:)([^"/]+\.bmp)"', source))
    records = []
    for name in sorted(names):
        raw = original.get('/data/' + name)
        paths = sorted((repo / 'resources').rglob(name))
        if raw is None or not paths:
            records.append(dict(name=name, original_exists=raw is not None, current_exists=bool(paths)))
            continue
        for path in paths:
            current = path.read_bytes()
            record = dict(name=name, current=str(path.relative_to(repo)),
                          original_sha256=hashlib.sha256(raw).hexdigest(),
                          current_sha256=hashlib.sha256(current).hexdigest(),
                          bytes_equal=raw == current)
            if name.endswith('.png') and raw != current:
                before, after = rgba(raw), rgba(current)
                record['rgba_equal'] = before == after
                record['original_size'] = before[:2]
                record['current_size'] = after[:2]
                if before[:2] == after[:2]:
                    record['changed_channels'] = sum(a != b for a, b in zip(before[2], after[2]))
            if name == 'pc_Console.bmp':
                header = (repo / 'src/lib/designer/legacy/aulegacyshiphelm.h').read_text()
                compressed = base64.b64decode(''.join(re.findall(r'"([A-Za-z0-9+/=]+)"', header)))
                record['legacy_override_exact'] = zlib.decompress(compressed[4:]) == raw
            if name == 'txSpacecraft0Lit.png':
                header = (repo / 'src/lib/designer/legacy/aulegacyshipatlas.h').read_text()
                restored = base64.b64decode(''.join(re.findall(r'"([A-Za-z0-9+/=]+)"', header)))
                record['legacy_override_exact'] = restored == raw
            records.append(record)
    return records


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('client_directory', type=Path)
    parser.add_argument('--repo', type=Path, default=Path('.'))
    args = parser.parse_args()
    print(json.dumps(inspect(args.client_directory, args.repo), indent=2))
