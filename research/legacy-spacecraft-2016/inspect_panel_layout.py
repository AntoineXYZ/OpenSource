#!/usr/bin/env python3
"""Read original panel constructor strings and layout-call assembly to stdout.

Uses objdump as a static disassembler; never loads or executes the client.
The large AuTabDesign constructor exceeded the MCP decompiler time limit.
These direct assembly call arguments preserve independent layout evidence.
"""
import argparse
import hashlib
import json
import re
import struct
import subprocess
from pathlib import Path


def inspect(executable):
    data = executable.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    count = struct.unpack_from('<H', data, pe + 6)[0]
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    sections = [struct.unpack_from('<8sIIII', data, pe + 24 + optional_size + i * 40)
                for i in range(count)]
    image_base = struct.unpack_from('<Q', data, pe + 24 + 24)[0]

    def string(address):
        for _, _, rva, size, offset in sections:
            if rva <= address - image_base < rva + size:
                start = offset + address - image_base - rva
                end = data.find(b'\0', start)
                raw = data[start:end]
                if len(raw) >= 2 and all(32 <= c < 127 or c in (9, 10, 13) for c in raw):
                    return raw.decode('ascii')
        return ''

    def disassemble(start, end):
        assembly = subprocess.check_output(
            ['objdump', '-d', '-M', 'intel', '--start-address=' + start,
             '--stop-address=' + end, str(executable)], text=True)
        result = []
        for line in assembly.splitlines():
            pieces = line.split('\t')
            if len(pieces) >= 3:
                result.append((pieces[0].strip().rstrip(':'), pieces[-1]))
        return result

    constructor = disassemble('0x1402c91e0', '0x1402d2197')
    tips = disassemble('0x1402e7d98', '0x1402e80e6')
    strings, help_strings, layout = [], [], []
    for body, target in ((constructor, strings), (tips, help_strings)):
        for index, (address, instruction) in enumerate(body):
            match = re.search(r'lea.*# 0x([0-9a-f]+)', instruction)
            if match:
                location = int(match[1], 16)
                value = string(location)
                if value:
                    target.append(dict(reference='0x' + address,
                                       address=hex(location), value=value))
            if target is strings and ('# 0x14043e810' in instruction or
                                      '# 0x14043e818' in instruction):
                layout.append(dict(call='0x' + address,
                                   assembly=['%s: %s' % pair
                                             for pair in body[max(0, index - 12):index + 1]]))
    return dict(source=executable.name, source_sha256=hashlib.sha256(data).hexdigest(),
                constructor='0x1402c91e0',
                provenance='Original x64 LEA strings and QGridLayout::addWidget call arguments; no client execution',
                constructor_strings=strings, offline_spacecraft_help=help_strings,
                layout_calls=layout)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    args = parser.parse_args()
    print(json.dumps(inspect(args.executable), indent=2))
