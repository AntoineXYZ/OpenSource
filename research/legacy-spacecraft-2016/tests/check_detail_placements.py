#!/usr/bin/env python3
"""Independent integer-grid placement audit; excludes spacing and model footprints."""
import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from inspect_ship import parse_ship


def relation(polygon, point):
    """0 outside, 1 inside, 2 exactly on edge; integer cross products."""
    x, y = point
    odd = False
    for i, (ax, ay) in enumerate(polygon):
        bx, by = polygon[(i + 1) % len(polygon)]
        if ((x - ax) * (by - ay) == (y - ay) * (bx - ax)
                and min(ax, bx) <= x <= max(ax, bx)
                and min(ay, by) <= y <= max(ay, by)):
            return 2
        if (ay > y) != (by > y) and x < (bx - ax) * (y - ay) / (by - ay) + ax:
            odd = not odd
    return int(odd)


def audit(design, definitions):
    failures = []
    for detail in design['details']:
        dtype = detail['offset_0x01_u8']
        placement = definitions[dtype]['placement']
        # Stairs and whole-design extents require independent footprint logic.
        if placement in (-6, -9):
            raise ValueError('This placement-only audit does not cover stairs/pivots')
        matches = []
        valid = True
        for room in design['rooms']:
            if room['deck'] != detail['deck']:
                continue
            rel = relation([v['grid_xy'] for v in room['vertices']], detail['grid_xy'])
            if not rel:
                continue
            matches.append(room)
            room_type = room['room_type']
            bridge = room_type == 2 and definitions[dtype]['bridge_allowed']
            if room_type == 2 and not bridge:
                valid = False
            if (placement >= 0 and room_type != placement and not bridge
                    and not (dtype in (74, 75) and room_type == 10)):
                valid = False
            if placement >= 0 and dtype not in (10, 14, 16, 18, 23, 99) and rel == 2:
                valid = False
            if placement in (-1, -4, -5, -10) and rel == 2:
                valid = False
            if placement in (-2, -3) and rel != 2:
                valid = False
            if placement == -1 and room_type not in (1, 4, 7, 19):
                valid = False
        if placement in (-4, -5):
            below = [r for r in design['rooms'] if r['deck'] == detail['deck'] - 1
                     and relation([v['grid_xy'] for v in r['vertices']], detail['grid_xy']) == 1]
            valid &= len(matches) + len(below) == (2 if placement == -4 else 1)
        elif placement == -3:
            valid &= len(matches) == 2
        elif placement == -7:
            valid &= len(matches) > 0
        else:
            valid &= len(matches) == 1
        if not valid:
            failures.append(dict(detail_id=detail['detail_id'], detail_type=dtype))
    return failures


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('ships', type=Path, nargs='+')
    args = parser.parse_args()
    table_path = Path(__file__).resolve().parents[1] / 'detail-types.json'
    definitions = {d['detail_type']: d for d in json.loads(table_path.read_text())['detail_types']}
    for path in args.ships:
        design = parse_ship(path.read_bytes())
        failures = audit(design, definitions)
        print(f'{path.name}: {len(design["details"])} placement checks; {len(failures)} violations')
        if failures:
            raise SystemExit(json.dumps(failures))
