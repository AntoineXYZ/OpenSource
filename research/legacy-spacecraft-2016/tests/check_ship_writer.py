#!/usr/bin/env python3
"""Audit 2016 save bytes; optionally compare an already-built native writer.

The reference uses struct/UTF-16, independently of QDataStream. Without --native
this checks the recovered format against original samples, not the C++ binary.
It never writes the samples or invokes a compiler.
"""

import argparse
from collections import OrderedDict
import copy
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

RESEARCH = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(RESEARCH))
from inspect_ship import parse_ship

DEFAULT_SLOTS = (234, 259, 237, 238, 235, 239, 263, 240)


def which_hash(key):
    mask = (1 << 64) - 1
    value = (((key & 255) ^ 0xcbf29ce484222325) * 0x100000001b3) & mask
    value = ((value ^ (key >> 8)) * 0x8a97b0004e7feab) & mask
    return (value >> 32) ^ value


def reconcile_modules(pairs, required, default):
    """Recover MSVC's wire iteration order independently using bucket groups."""
    groups = OrderedDict()
    count = 8
    seen = set()
    additions = [{"first": key, "second_raw": default, "second_normalized": default}
                 for key in sorted(required)]
    for pair in [*pairs, *additions]:
        key = pair["first"]
        if key in seen:
            continue
        seen.add(key)
        groups.setdefault(which_hash(key) & (count - 1), []).insert(0, pair)
        if len(seen) > count:
            count *= 8 if count < 512 else 2
            old_order = [entry for bucket in groups.values() for entry in bucket]
            groups = OrderedDict()
            for entry in old_order:
                groups.setdefault(which_hash(entry["first"]) & (count - 1), []).insert(0, entry)
    return [entry for bucket in groups.values() for entry in bucket if entry["first"] in required]


def prepare_for_save(parsed):
    prepared = copy.deepcopy(parsed)
    rooms = {}
    bays, transporters = set(), set()
    for room in parsed["rooms"]:
        if not room["retained_by_legacy_reader"]:
            continue
        rooms.setdefault(room["room_id"], room)
        if room["room_type"] == 0x15:
            bays.add(room["offset_0x08_u8"])
    for detail in parsed["details"]:
        room = rooms.get(detail["offset_0x28_u16"])
        if room is None:
            continue
        if detail["offset_0x01_u8"] == 0x14:
            transporters.add(((room["deck"] & 255) << 8) | room["offset_0x08_u8"])
        elif detail["offset_0x01_u8"] in (0x17, 0x63):
            bays.add(room["offset_0x08_u8"])
    modules = prepared["modules"] or {"first_pairs": [], "scalar_u16_slots": DEFAULT_SLOTS, "second_pairs": []}
    modules["first_pairs"] = reconcile_modules(modules["first_pairs"], bays, 0x105)
    modules["second_pairs"] = reconcile_modules(modules["second_pairs"], transporters, 0xf6)
    prepared["modules"] = modules
    prepared["details"].sort(key=lambda d: (d["deck"], d["offset_0x28_u16"], d["detail_id"]))
    return prepared


def compare_legacy_save(expected, legacy_bytes):
    """Project only session identity, then compare every byte, not just geometry."""
    actual = parse_ship(legacy_bytes)
    comparable = parse_ship(expected)
    for field in ("design_id", "architect_id", "architect_name"):
        print(f"  identity {field}: ours={comparable['header'][field]!r}, 2016={actual['header'][field]!r}")
        comparable["header"][field] = actual["header"][field]
    for record in comparable["rooms"] + comparable["details"]:
        record["design_id"] = actual["header"]["design_id"]
    adjusted = reference_save(comparable)
    assert adjusted == legacy_bytes, "2016 save differs beyond design/architect identity"
    print("PASS actual 2016 save: every byte matches with captured session identity")
    print(f"  2016 SHA-256 {hashlib.sha256(adjusted).hexdigest()}")


def qstring(value):
    if value is None:
        return b"\xff\xff\xff\xff"
    encoded = value.encode("utf-16-be", errors="surrogatepass")
    return struct.pack(">I", len(encoded)) + encoded


def reference_save(parsed):
    h = parsed["header"]
    data = bytearray(struct.pack(">I", h["design_id"]))
    data += qstring(h["name"])
    data += struct.pack(">IBBB", h["architect_id"], h["streamline"], h["armor"], 0x20 | h["state"])
    data += qstring(h["architect_name"])
    data += struct.pack(">BIBB", h["minimum_tech_level"], h.get("hull_volume", 0),
                        h.get("structure", 0), h.get("tech_level", 0))
    modules = parsed["modules"] or {"first_pairs": [], "scalar_u16_slots": DEFAULT_SLOTS, "second_pairs": []}
    data += struct.pack(">I", len(modules["first_pairs"]))
    for pair in modules["first_pairs"]:
        data += struct.pack(">HH", pair["first"], pair["second_normalized"])
    data += struct.pack(">8H", *modules["scalar_u16_slots"])
    data += struct.pack(">I", len(modules["second_pairs"]))
    for pair in modules["second_pairs"]:
        data += struct.pack(">HH", pair["first"], pair["second_raw"])
    rooms = [room for room in parsed["rooms"] if room["retained_by_legacy_reader"]]
    data += struct.pack(">H", len(rooms))
    for room in rooms:
        if room["room_type"] == 0x18:
            raise ValueError("Dome compression requires native float geometry; use native --self-test for those fixtures")
        data += struct.pack(">IHbBBH", room["design_id"], room["room_id"], room["deck"],
                            room["room_type"], room["offset_0x08_u8"], len(room["vertices"]))
        data += b"".join(struct.pack(">H", point["packed_xy"]) for point in room["vertices"])
    data += struct.pack(">I", len(parsed["details"]))
    for detail in parsed["details"]:
        data += struct.pack(">bBBIIHHH", detail["deck"], detail["offset_0x01_u8"],
                            int(detail["bool_normalized"]), detail["detail_id"], detail["design_id"],
                            detail["packed_xy"], detail["offset_0x28_u16"], detail["offset_0x2a_u16_normalized"])
    return bytes(data)


def expected_sample_changes(parsed):
    """Independent offset audit for canonical v2 samples (no empty/dome rooms)."""
    assert parsed["header"]["version"] == 2
    assert all(room["retained_by_legacy_reader"] and room["room_type"] != 0x18 for room in parsed["rooms"])
    assert all(pair["second_raw"] == pair["second_normalized"] for pair in parsed["modules"]["first_pairs"])
    expected = {}
    for detail in parsed["details"]:
        offset = detail["stream_offset"]
        if detail["bool_raw_u8"] != int(detail["bool_normalized"]):
            expected[offset + 2] = int(detail["bool_normalized"])
        raw = detail["offset_0x2a_u16_raw"]
        normalized = detail["offset_0x2a_u16_normalized"]
        if raw != normalized:
            assert (raw, normalized) in ((0xf9, 0xf8), (0xfb, 0xfa))
            expected[offset + 16] = normalized
    return expected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="*", type=Path, help="original sample .ship files; defaults to recorded baseline paths")
    parser.add_argument("--native", type=Path, help="already-built native_ship_writer executable (never builds)")
    parser.add_argument("--legacy-save", type=Path, help="actual 2016 save of the single input file; compare all bytes after identity projection")
    args = parser.parse_args()
    paths = args.paths or [Path(item["path"]) for item in json.loads((RESEARCH / "sample-inspection.json").read_text())["files"]]
    if args.legacy_save and len(paths) != 1:
        parser.error("--legacy-save requires exactly one input .ship path")
    for path in paths:
        original = path.read_bytes()
        parsed = parse_ship(original)
        raw = reference_save(parsed)
        assert len(raw) == len(original), f"Unexpected raw serializer size change: {path}"
        actual_changes = {i: after for i, (before, after) in enumerate(zip(original, raw)) if before != after}
        assert actual_changes == expected_sample_changes(parsed), f"Unexpected byte changes: {path}"
        expected = reference_save(prepare_for_save(parsed))
        assert reference_save(parse_ship(expected)) == expected, "Serialized records are not stable"
        if args.native:
            with tempfile.TemporaryDirectory(prefix="legacy-ship-writer-") as folder:
                output = Path(folder) / "saved.ship"
                subprocess.run([str(args.native.resolve()), "--rewrite", str(path.resolve()), str(output)], check=True)
                saved = output.read_bytes()
                assert saved == expected, f"Native output differs from recovered writer: {path}"
        label = "native + reference" if args.native else "reference only"
        print(f"PASS {path.name}: {len(expected)} bytes after designer preparation; {len(actual_changes)} normalized ID/bool bytes ({label})")
        print(f"  saved SHA-256 {hashlib.sha256(expected).hexdigest()}")
        if args.legacy_save:
            compare_legacy_save(expected, args.legacy_save.read_bytes())
    return 0


if __name__ == "__main__":
    sys.exit(main())
