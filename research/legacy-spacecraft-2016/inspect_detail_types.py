#!/usr/bin/env python3
"""Read the detail-type prefix of a 2016 GD cache; never modifies its input."""
import argparse
import hashlib
import json
import struct
from pathlib import Path


def read_types(data):
    if data[:2] != bytes((0x18, 0x3a)):
        raise ValueError('Expected 2016 GD version 0x3a')
    offset = 2

    def number(fmt):
        nonlocal offset
        result = struct.unpack_from('>' + fmt, data, offset)[0]
        offset += struct.calcsize('>' + fmt)
        return result

    def string():
        nonlocal offset
        size = number('I')
        if size == 0xffffffff:
            return None
        if size % 2 or offset + size > len(data):
            raise ValueError('Invalid GD QString')
        result = data[offset:offset + size].decode('utf-16-be')
        offset += size
        return result

    result = []
    for _ in range(number('H')):
        start = offset
        dtype = number('B')
        names = [string() for _ in range(4)]
        bridge = number('B')
        crew = number('H')
        placement = number('h')
        minimum_tl = number('B')
        mass = number('H')
        result.append(dict(stream_offset=start, detail_type=dtype,
                           name=names[0], bridge_allowed=bool(bridge),
                           placement=placement, crew=crew,
                           minimum_tech_level=minimum_tl, mass_kg=mass))
    return dict(gd_sha256=hashlib.sha256(data).hexdigest(),
                detail_table_end=offset, detail_types=result)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('gd', type=Path)
    args = parser.parse_args()
    print(json.dumps(read_types(args.gd.read_bytes()), indent=2))
