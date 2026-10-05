#!/usr/bin/env python3
"""Read-only 2016 ship-stat reference and recovered-table verification.

This runs no compiler or native code. The sample oracle independently uses
shoelace polygon area and GD data; it does not exercise the C++ implementation.
By default samples come from sample-inspection.json; explicit .ship paths may
also be supplied. Baselines are matched by SHA-256, not filename.
"""

import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import re
import struct

from inspect_ship import parse_ship


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
CONSOLES = {1, 4, 6, 7, 12, 13, 15, 17, 19, 73}
VEHICLE_MASS = {
    6: 5511, 7: 22046, 8: 22046, 77: 22046, 9: 55115,
    28: 6400, 82: 6400, 104: 66138, 132: 320, 133: 230, 231: 26455,
}


def f32(value):
    return struct.unpack("<f", struct.pack("<f", value))[0]


def check_table(metadata):
    source = (ROOT / "src/lib/data/designs/aulegacyshipstats.cpp").read_text()
    compiled_rows = [
        tuple(map(int, row))
        for row in re.findall(r"^\s*\{(\d+), (\d+), (\d+)\}, // \d+:", source, re.M)
    ]
    recovered_rows = [(row["mass_kg"], row["minimum_tech_level"], row["crew"]) for row in metadata]
    if compiled_rows != recovered_rows:
        raise AssertionError("C++ equipment table differs from recovered GD values")
    print(f"PASS: all {len(recovered_rows)} C++ mass/TL/crew rows match recovered GD")


def reference_stats(parsed, metadata):
    rooms = {}
    volumes = Counter()
    hull_volume = 0
    for room in parsed["rooms"]:
        if not room["vertices"]:
            continue
        if room["ellipse_controls"]:
            raise ValueError("The ship-only reference does not approximate building ellipse controls")
        rooms.setdefault(room["room_id"], room)
        points = [vertex["decoded_xyz"] for vertex in room["vertices"]]
        twice_area = sum(
            a[0] * b[1] - a[1] * b[0]
            for a, b in zip(points, points[1:] + points[:1])
        )
        volume = math.floor(abs(twice_area) * 4 + 0.5)
        hull_volume += volume
        volumes[room["room_type"]] += volume

    counts = Counter()
    berths = Counter()
    mass = officer_stations = crew_stations = pilots = drivers = turrets = guards = 0
    for detail in parsed["details"]:
        detail_type = detail["offset_0x01_u8"]
        counts[detail_type] += 1
        if detail_type >= len(metadata):
            continue
        mass += int(f32(f32(metadata[detail_type]["mass_kg"]) * f32(2.2046225)))
        room = rooms.get(detail["offset_0x28_u16"])
        if 57 <= detail_type <= 64:
            commodity = detail["offset_0x2a_u16_normalized"]
            mass += VEHICLE_MASS.get(commodity, 0)
            if 6 <= commodity <= 9 or commodity == 231:
                pilots += 1
            else:
                drivers += 1
        if room:
            if detail_type == 2 or 25 <= detail_type <= 31:
                berths[room["room_type"]] += 2
            elif 65 <= detail_type <= 72:
                berths[room["room_type"]] += 1
            if detail_type in CONSOLES:
                if detail_type == 73:
                    officer_stations += 1
                else:
                    crew_stations += 1
            if detail_type in (21, 22) or 33 <= detail_type <= 40:
                turrets += 1
        if detail_type == 141:
            guards += 1
    return {
        "hull_volume": hull_volume, "equipment_mass_lb": mass,
        "crew_berths": berths[7], "officer_berths": berths[4],
        "passenger_berths": berths[19], "troop_berths": berths[1],
        "officer_stations": officer_stations, "crew_stations": crew_stations,
        "pilots": pilots, "drivers": drivers, "turrets": turrets, "guards": guards,
        "maneuver_units": counts[10], "life_support_units": counts[32],
        "fuel_volume": volumes[9], "hold_volume": volumes[5],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="*", type=Path)
    parser.add_argument("--require-samples", action="store_true",
                        help="Fail if a default sample path is unavailable")
    args = parser.parse_args()
    metadata = json.loads((HERE / "detail-types.json").read_text())["detail_types"]
    expected = json.loads((HERE / "ship-stats-baselines.json").read_text())
    check_table(metadata)
    paths = args.paths or [
        Path(entry["path"])
        for entry in json.loads((HERE / "sample-inspection.json").read_text())["files"]
    ]
    checked = 0
    for path in paths:
        if not path.exists() and not args.paths and not args.require_samples:
            print(f"SKIP unavailable sample: {path}")
            continue
        data = path.read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        result = reference_stats(parse_ship(data), metadata)
        baseline = expected.get(digest)
        if baseline:
            for key, value in baseline.items():
                if key != "file" and result[key] != value:
                    raise AssertionError(f"{path.name}: {key}: got {result[key]}, expected {value}")
            checked += 1
            print(f"PASS sample baseline: {path.name}")
        else:
            print(f"Unbaselined sample: {path.name} ({digest})")
        print(json.dumps(result, sort_keys=True))
    print(f"Verified {checked} known sample baselines; no native code or build was run.")


if __name__ == "__main__":
    main()

