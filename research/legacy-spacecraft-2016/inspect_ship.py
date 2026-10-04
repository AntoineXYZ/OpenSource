#!/usr/bin/env python3
"""Inspect the recovered 2016 .ship stream without modifying input files.

The field order is based on DnSpaceShip::Rx, DnRoomArray's stream reader,
and DnDetailArray's stream reader in the accompanying pseudocode evidence.
Unknown fields use object offsets as labels, not assumed semantic names.
This is an experimental format inspector, not a modern .SoH file reader.
"""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import sys


MAX_FILE_BYTES = 64 * 1024 * 1024
MAX_DECODED_RECORDS = 250_000
XY_SCALE = 2.8284271245
DECK_SCALE = 8.0
FIRST_PAIR_REMAP = {0xFD: 0xFC, 0xFF: 0xFE}
DETAIL_FINAL_REMAP = {0xF9: 0xF8, 0xFB: 0xFA}
HEADER_FIELD_PROVENANCE = {
    "architect_id": "AuId uses a four-byte stream representation; confirmed by legacy AuGlobal operators",
    "streamline": "DnSpaceShip +0x14; getter at AuData13.dll:0x180031080",
    "armor": "DnSpaceShip +0x15; SetArmor at AuData13.dll:0x18017e3a0",
    "architect_name": "DnSpaceShip +0x20; getter at AuData13.dll:0x1800351c0",
    "minimum_tech_level": "DnSpaceShip +0x30; getter at AuData13.dll:0x180034b70",
    "hull_volume": "DnSpaceShip +0xd0; getter at AuData13.dll:0x180031410",
    "structure": "DnSpaceShip +0xdd; getter at AuData13.dll:0x180046910",
    "tech_level": "DnSpaceShip +0xde; getter at AuData13.dll:0x180046d50",
}


class ShipFormatError(ValueError):
    """The input cannot be consumed using the recovered stream layout."""


class Reader:
    def __init__(self, data):
        self.data = data
        self.offset = 0
        self.records_remaining = MAX_DECODED_RECORDS

    @property
    def remaining(self):
        return len(self.data) - self.offset

    def fail(self, message):
        raise ShipFormatError(f"offset 0x{self.offset:x}: {message}")

    def take(self, size, label):
        if size < 0 or size > self.remaining:
            self.fail(f"{label}: need {size} bytes, only {self.remaining} remain")
        start = self.offset
        self.offset += size
        return self.data[start:self.offset]

    def number(self, fmt, label):
        return struct.unpack(">" + fmt, self.take(struct.calcsize(">" + fmt), label))[0]

    def u8(self, label):
        return self.number("B", label)

    def i8(self, label):
        return self.number("b", label)

    def u16(self, label):
        return self.number("H", label)

    def u32(self, label):
        return self.number("I", label)

    def qstring(self, label):
        size = self.u32(label + ".byte_length")
        if size == 0xFFFFFFFF:
            return None
        if size % 2:
            self.fail(f"{label}: odd UTF-16 byte length {size}")
        # Preserve lone UTF-16 surrogates rather than silently replacing data.
        return self.take(size, label).decode("utf-16-be", errors="surrogatepass")

    def checked_count(self, count, minimum_record_bytes, label):
        if count > self.remaining // minimum_record_bytes:
            self.fail(
                f"{label}: count {count} requires at least "
                f"{count * minimum_record_bytes} bytes, only {self.remaining} remain"
            )
        if count > self.records_remaining:
            self.fail(f"{label}: exceeds combined {MAX_DECODED_RECORDS}-record safety limit")
        self.records_remaining -= count
        return count


def signed_byte(value):
    return value - 256 if value >= 128 else value


def point_from_packed(packed, deck):
    x = signed_byte(packed >> 8)
    y = signed_byte(packed & 0xFF)
    return {
        "packed_xy": packed,
        "grid_xy": [x, y],
        "decoded_xyz": [x * XY_SCALE, y * XY_SCALE, deck * DECK_SCALE],
    }


def read_pairs(reader, label, remap=None):
    count = reader.checked_count(reader.u32(label + ".count"), 4, label)
    pairs = []
    for index in range(count):
        first = reader.u16(f"{label}[{index}].first")
        second = reader.u16(f"{label}[{index}].second")
        pair = {"first": first, "second_raw": second}
        if remap is not None:
            pair["second_normalized"] = remap.get(second, second)
        pairs.append(pair)
    return pairs


def parse_ship(data):
    """Return complete decoded records, requiring exact consumption of input."""
    if len(data) > MAX_FILE_BYTES:
        raise ShipFormatError(f"input exceeds {MAX_FILE_BYTES}-byte safety limit")
    reader = Reader(data)
    header = {
        "design_id": reader.u32("header.design_id"),
        "name": reader.qstring("header.name"),
        "architect_id": reader.u32("header.architect_id"),
        "streamline": reader.u8("header.streamline"),
        "armor": reader.u8("header.armor"),
    }
    packed = reader.u8("header.version_state")
    version = packed >> 4
    header.update({
        "version_state_raw": packed,
        "version": version,
        "state": packed & 0x0F,
        "architect_name": reader.qstring("header.architect_name"),
    })
    if version < 2:
        header["legacy_discarded_qstring"] = reader.qstring("header.legacy_discarded")
    header["minimum_tech_level"] = reader.u8("header.minimum_tech_level")
    if version > 1:
        header["hull_volume"] = reader.u32("header.hull_volume")
        header["structure"] = reader.u8("header.structure")
        header["tech_level"] = reader.u8("header.tech_level")
    modules = None
    if version != 0:
        modules = {
            "first_pairs": read_pairs(reader, "modules.first_pairs", FIRST_PAIR_REMAP),
            "scalar_u16_slots": [reader.u16(f"modules.scalar[{i}]") for i in range(8)],
            "second_pairs": read_pairs(reader, "modules.second_pairs"),
        }
    rooms_offset = reader.offset
    room_count = reader.checked_count(reader.u16("rooms.count"), 11, "rooms")
    rooms = []
    for index in range(room_count):
        start = reader.offset
        label = f"rooms[{index}]"
        room = {
            "stream_offset": start,
            "design_id": reader.u32(label + ".design_id"),
            "room_id": reader.u16(label + ".room_id"),
            "deck": reader.i8(label + ".deck"),
            "room_type": reader.u8(label + ".room_type"),
            "offset_0x08_u8": reader.u8(label + ".offset_0x08"),
        }
        count = reader.checked_count(reader.u16(label + ".vertex_count"), 2, label + ".vertices")
        room["stored_vertex_count"] = count
        room["retained_by_legacy_reader"] = count != 0
        room["ellipse_controls"] = room["room_type"] == 0x18 and count == 3
        room["vertices"] = [
            point_from_packed(reader.u16(f"{label}.vertices[{i}]"), room["deck"])
            for i in range(count)
        ]
        room["stream_size"] = reader.offset - start
        rooms.append(room)
    details_offset = reader.offset
    detail_count = reader.checked_count(reader.u32("details.count"), 17, "details")
    details = []
    for index in range(detail_count):
        label = f"details[{index}]"
        detail = {
            "stream_offset": reader.offset,
            "deck": reader.i8(label + ".deck"),
            "offset_0x01_u8": reader.u8(label + ".offset_0x01"),
            "bool_raw_u8": reader.u8(label + ".bool"),
            "detail_id": reader.u32(label + ".detail_id"),
            "design_id": reader.u32(label + ".design_id"),
        }
        detail["bool_normalized"] = bool(detail["bool_raw_u8"])
        detail.update(point_from_packed(reader.u16(label + ".packed_xy"), detail["deck"]))
        detail["offset_0x28_u16"] = reader.u16(label + ".offset_0x28")
        final = reader.u16(label + ".offset_0x2a")
        detail["offset_0x2a_u16_raw"] = final
        detail["offset_0x2a_u16_normalized"] = DETAIL_FINAL_REMAP.get(final, final)
        details.append(detail)
    if reader.remaining:
        reader.fail(f"unexpected trailing data: {reader.remaining} bytes")
    return {
        "header": header,
        "modules": modules,
        "rooms": rooms,
        "details": details,
        "stream": {
            "byte_order": "big-endian",
            "qdatastream_version": "Qt_4_0 (integer 7)",
            "rooms_count_offset": rooms_offset,
            "details_count_offset": details_offset,
            "bytes_consumed": reader.offset,
            "exact_eof": True,
        },
    }


def histogram(values, hexadecimal=False):
    counts = Counter(values)
    return {
        (f"0x{key:02x}" if hexadecimal else str(key)): counts[key]
        for key in sorted(counts)
    }


def bounds(points):
    if not points:
        return None
    return {
        "minimum": [min(point[axis] for point in points) for axis in range(3)],
        "maximum": [max(point[axis] for point in points) for axis in range(3)],
    }


def summarize(parsed):
    rooms = parsed["rooms"]
    details = parsed["details"]
    modules = parsed["modules"]
    return {
        "header": parsed["header"],
        "modules": modules,
        "stream": parsed["stream"],
        "counts": {
            "rooms": len(rooms),
            "rooms_retained_by_legacy_reader": sum(room["retained_by_legacy_reader"] for room in rooms),
            "empty_boundary_rooms_discarded_by_legacy_reader": sum(not room["retained_by_legacy_reader"] for room in rooms),
            "stored_vertices": sum(room["stored_vertex_count"] for room in rooms),
            "ellipse_control_rooms": sum(room["ellipse_controls"] for room in rooms),
            "details": len(details),
        },
        "histograms": {
            "room_type": histogram((room["room_type"] for room in rooms), True),
            "room_deck": histogram(room["deck"] for room in rooms),
            "detail_deck": histogram(detail["deck"] for detail in details),
            "detail_offset_0x28_u16": histogram((detail["offset_0x28_u16"] for detail in details), True),
            "detail_offset_0x2a_raw": histogram((detail["offset_0x2a_u16_raw"] for detail in details), True),
            "detail_offset_0x2a_normalized": histogram((detail["offset_0x2a_u16_normalized"] for detail in details), True),
        },
        "stored_vertex_control_bounds": bounds([
            vertex["decoded_xyz"] for room in rooms for vertex in room["vertices"]
        ]),
        "detail_position_bounds": bounds([detail["decoded_xyz"] for detail in details]),
        "geometry_note": "Bounds cover stored vertices/control points only; ellipse-control rooms are not expanded or interpreted as ordinary polygons.",
        "normalizations": {
            "module_first_pair_second_values_changed": 0 if modules is None else sum(
                pair["second_raw"] != pair["second_normalized"] for pair in modules["first_pairs"]
            ),
            "detail_final_values_changed": sum(
                detail["offset_0x2a_u16_raw"] != detail["offset_0x2a_u16_normalized"]
                for detail in details
            ),
        },
    }


def inspect_file(path, full=False):
    with path.open("rb") as source:
        data = source.read(MAX_FILE_BYTES + 1)
    parsed = parse_ship(data)
    report = {
        "path": str(path),
        "size_bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "status": "parsed_exact_eof",
        "summary": summarize(parsed),
    }
    if full:
        report["records"] = parsed
    return report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", type=Path, help="2016 .ship files to read")
    parser.add_argument("--full", action="store_true", help="include every room, vertex/control point, and detail")
    args = parser.parse_args(argv)
    reports = []
    failed = False
    for path in args.paths:
        try:
            reports.append(inspect_file(path, args.full))
        except (OSError, ShipFormatError) as error:
            failed = True
            reports.append({"path": str(path), "status": "error", "error": str(error)})
    json.dump({
        "format": "experimental-2016-ship",
        "header_field_provenance": HEADER_FIELD_PROVENANCE,
        "limits": {"file_bytes": MAX_FILE_BYTES, "combined_records": MAX_DECODED_RECORDS},
        "files": reports,
    }, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
