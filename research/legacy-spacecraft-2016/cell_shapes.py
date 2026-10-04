#!/usr/bin/env python3
"""Read-only reference for 2016 DaDesignPlane::GetFillShapeAt.

This models the recovered function, not the room-painting/editing operation.
Coordinates are feet. See CELL-SHAPES.txt for evidence and limitations.
No game, sample-file or database writes are performed.
"""

import argparse
import json
import math
import struct


GRID = float.fromhex("0x1.6a09e6676c64ap+2")  # 5.656854249 feet
HALF_GRID = GRID / 2.0
HORIZONTAL, DIAGONAL_45, VERTICAL, DIAGONAL_135 = 1, 2, 4, 8
WEDGE_NAMES = ("east", "north", "west", "south")

# Each mode contains four clockwise vertex arrays indexed E, N, W, S.
# Integer coordinates are HALF-GRID units relative to the snapped cell origin.
# Order is the original append order; there is no repeated closing vertex.
SHAPES = {
    0x3: (
        ((0, 0), (2, 2), (4, 2), (2, 0)),
        ((0, 0), (-2, 0), (0, 2), (2, 2)),
        ((0, 0), (-2, 0), (0, 2), (2, 2)),
        ((0, 0), (2, 2), (4, 2), (2, 0)),
    ),
    0x5: (((0, 0), (0, 2), (2, 2), (2, 0)),) * 4,
    0x6: (
        ((0, 0), (2, 2), (2, 0), (0, -2)),
        ((0, 0), (0, 2), (2, 4), (2, 2)),
        ((0, 0), (0, 2), (2, 4), (2, 2)),
        ((0, 0), (2, 2), (2, 0), (0, -2)),
    ),
    0x7: (
        ((0, 0), (2, 2), (2, 0)),
        ((0, 0), (0, 2), (2, 2)),
        ((0, 0), (0, 2), (2, 2)),
        ((0, 0), (2, 2), (2, 0)),
    ),
    0x9: (
        ((0, 2), (2, 2), (4, 0), (2, 0)),
        ((0, 2), (2, 2), (4, 0), (2, 0)),
        ((0, 2), (2, 0), (0, 0), (-2, 2)),
        ((0, 2), (2, 0), (0, 0), (-2, 2)),
    ),
    0xA: (
        ((1, 1), (2, 2), (3, 1), (2, 0)),
        ((1, 1), (0, 2), (1, 3), (2, 2)),
        ((1, 1), (0, 0), (-1, 1), (0, 2)),
        ((1, 1), (2, 0), (1, -1), (0, 0)),
    ),
    0xB: (
        ((1, 1), (2, 2), (3, 1), (2, 0)),
        ((1, 1), (0, 2), (2, 2)),
        ((1, 1), (0, 0), (-1, 1), (0, 2)),
        ((1, 1), (2, 0), (0, 0)),
    ),
    0xC: (
        ((0, 2), (0, 4), (2, 2), (2, 0)),
        ((0, 2), (0, 4), (2, 2), (2, 0)),
        ((0, 2), (2, 0), (2, -2), (0, 0)),
        ((0, 2), (2, 0), (2, -2), (0, 0)),
    ),
    0xD: (
        ((2, 0), (0, 2), (2, 2)),
        ((2, 0), (0, 2), (2, 2)),
        ((2, 0), (0, 0), (0, 2)),
        ((2, 0), (0, 0), (0, 2)),
    ),
    0xE: (
        ((1, 1), (2, 2), (2, 0)),
        ((1, 1), (0, 2), (1, 3), (2, 2)),
        ((1, 1), (0, 0), (0, 2)),
        ((1, 1), (2, 0), (1, -1), (0, 0)),
    ),
    0xF: (
        ((1, 1), (2, 2), (2, 0)),
        ((1, 1), (0, 2), (2, 2)),
        ((1, 1), (0, 0), (0, 2)),
        ((1, 1), (2, 0), (0, 0)),
    ),
}


def cell_index(value):
    """Original finite/in-range truncation and negative decrement, NOT floor."""
    scaled = value / GRID
    if not math.isfinite(scaled) or not -2147483647 <= scaled < 2147483648:
        raise ValueError("coordinate outside the reference's finite int32 domain")
    return int(scaled) - int(scaled < 0.0)


def wedge(fx, fy):
    """Exact original comparison order, including diagonal and center ties."""
    if fx < 0.5:
        if fy <= 0.5:
            return 2 + int(0.5 - fx <= 0.5 - fy)
        return 1 + int(fy - 0.5 <= 0.5 - fx)
    if fy <= 0.5:
        return 0 if 0.5 - fy < fx - 0.5 else 3
    return int(fx < fy)


def fill_shape(mask, x, y, z=0.0):
    """Return original-order 3D points; Z passes through without deck rounding.

    mask must be a byte, matching the original object's stored field. Unused
    byte values yield an empty polygon. Undefined/out-of-range native int32
    conversion and non-finite coordinates are deliberately not emulated.
    """
    if not isinstance(mask, int) or not 0 <= mask <= 255:
        raise ValueError("mask must be an unsigned byte")
    if mask not in SHAPES:
        return []
    if not math.isfinite(z):
        raise ValueError("Z must be finite")
    ix, iy = cell_index(x), cell_index(y)
    sector = wedge(x / GRID - ix, y / GRID - iy)
    return [
        (vx * HALF_GRID + ix * GRID, vy * HALF_GRID + iy * GRID, z)
        for vx, vy in SHAPES[mask][sector]
    ]


def table():
    return {
        "source": "AuAtom13.dll!DaDesignPlane::GetFillShapeAt@0x1802fd190",
        "grid_spacing_feet": GRID,
        "vertex_unit_feet": HALF_GRID,
        "vertex_order": "clockwise, original append order, open array",
        "bits": {"horizontal": 1, "diagonal_45": 2, "vertical": 4, "diagonal_135": 8},
        "wedge_order": WEDGE_NAMES,
        "masks": {str(mask): SHAPES.get(mask, ((),) * 4) for mask in range(16)},
        "other_byte_masks": "empty",
    }


def _clip(poly, nx, ny, bound):
    """Independent geometric oracle: clip an arbitrary convex polygon."""
    out = []
    for start, end in zip(poly, poly[1:] + poly[:1]):
        a = start[0] * nx + start[1] * ny - bound
        b = end[0] * nx + end[1] * ny - bound
        if a <= 0:
            out.append(start)
        if (a < 0 < b) or (b < 0 < a):
            t = a / (a - b)
            out.append((start[0] + t * (end[0] - start[0]),
                        start[1] + t * (end[1] - start[1])))
    return out


def _geometric_cell(mask, x, y):
    # Work in full-grid units. Each enabled line family bounds one strip.
    poly = [(-3.0, -3.0), (4.0, -3.0), (4.0, 4.0), (-3.0, 4.0)]
    for bit, nx, ny in ((1, 0, 1), (2, -1, 1), (4, 1, 0), (8, 1, 1)):
        if mask & bit:
            low = math.floor(nx * x + ny * y)
            poly = _clip(poly, nx, ny, low + 1)
            poly = _clip(poly, -nx, -ny, -low)
    return poly


def self_test():
    """Check tables against geometry, constants and explicit boundary fixtures."""
    assert struct.pack(">d", GRID).hex() == "4016a09e6676c64a"
    assert struct.pack(">d", HALF_GRID).hex() == "4006a09e6676c64a"
    assert struct.pack(">d", GRID * 1.5).hex() == "4020f876ccd914b8"
    assert struct.pack(">d", GRID * 2.0).hex() == "4026a09e6676c64a"

    # Expected classification transcribed from the assembly's SETBE/SETA/CMOVA.
    fixtures = [
        (0.875, 0.5, 0), (0.5, 0.875, 1),
        (0.125, 0.5, 2), (0.5, 0.125, 3),
        (0.25, 0.25, 3), (0.25, 0.75, 2),
        (0.75, 0.75, 0), (0.75, 0.25, 3),
        (0.5, 0.5, 3), (0.0, 0.0, 3),
        (0.0, 1.0, 2), (1.0, 1.0, 0), (1.0, 0.0, 3),
    ]
    for x, y, expected in fixtures:
        assert wedge(x, y) == expected, (x, y, expected)
    assert wedge(math.nextafter(0.75, 0.0), 0.75) == 1
    assert wedge(math.nextafter(0.75, 1.0), 0.75) == 0
    # The subtraction rounds away this single-ULP movement: preserve the tie.
    assert wedge(math.nextafter(0.25, 0.0), 0.25) == 3
    assert wedge(math.nextafter(math.nextafter(0.25, 0.0), 0.0), 0.25) == 2
    assert wedge(math.nextafter(0.25, 1.0), 0.25) == 3
    assert wedge(math.nextafter(0.25, 0.0), 0.75) == 2
    assert wedge(math.nextafter(0.25, 1.0), 0.75) == 1
    assert wedge(math.nextafter(0.75, 0.0), 0.25) == 3
    assert wedge(math.nextafter(0.75, 1.0), 0.25) == 0

    for value, expected in ((-2 * GRID, -3), (-GRID, -2),
                            (-0.5 * GRID, -1), (-0.0, 0),
                            (0.0, 0), (0.5 * GRID, 0), (GRID, 1)):
        assert cell_index(value) == expected, (value, expected)
    assert cell_index(math.nextafter(-GRID, -math.inf)) == -2
    assert cell_index(math.nextafter(-GRID, math.inf)) == -1
    assert cell_index(math.nextafter(GRID, -math.inf)) == 0
    assert cell_index(math.nextafter(GRID, math.inf)) == 1
    assert fill_shape(5, -GRID, -GRID, 13.25) == [
        (-2 * GRID, -2 * GRID, 13.25), (-2 * GRID, -GRID, 13.25),
        (-GRID, -GRID, 13.25), (-GRID, -2 * GRID, 13.25),
    ]

    for mask in range(256):
        result = fill_shape(mask, 0.2 * GRID, 0.3 * GRID, -7.0)
        assert bool(result) == (mask in SHAPES), mask
        assert all(z == -7.0 for _, _, z in result)

    # Interior points exclude grid lines: geometric clipping has no tie policy.
    checked = 0
    for mask, sectors in SHAPES.items():
        for points in sectors:
            cross_sum = sum(ax * by - ay * bx
                            for (ax, ay), (bx, by)
                            in zip(points, points[1:] + points[:1]))
            assert cross_sum < 0, (mask, points)
        for nx in range(1, 16):
            for ny in range(1, 16):
                if nx == ny or nx + ny == 16:
                    continue
                x, y = nx / 16.0, ny / 16.0
                expected = {(round(vx, 10), round(vy, 10))
                            for vx, vy in _geometric_cell(mask, x, y)}
                observed = {(vx / 2.0, vy / 2.0)
                            for vx, vy in SHAPES[mask][wedge(x, y)]}
                assert observed == expected, (mask, x, y, observed, expected)
                # Same cell translated across positive and negative coordinates.
                for dx, dy in ((0, 0), (-2, -3), (7, -4), (-5, 6)):
                    result = fill_shape(mask, (x + dx) * GRID, (y + dy) * GRID, 24.0)
                    normalized = {(round(px / GRID - dx, 10), round(py / GRID - dy, 10))
                                  for px, py, _ in result}
                    assert normalized == expected, (mask, dx, dy, x, y)
                checked += 1
    return {"geometric_interior_cases": checked, "translated_cases": checked * 4,
            "byte_masks": 256, "explicit_wedge_fixtures": len(fixtures),
            "result": "passed", "native_binary_executed": False}


def atlas_svg():
    """A standalone vector diagram; positive Y points upward in every panel."""
    width, height = 950, 70 + len(SHAPES) * 145
    lines = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
             '<rect width="100%" height="100%" fill="#131820"/>',
             '<style>text{font-family:monospace;fill:#e7edf5;font-size:12px}.title{font-size:20px}.small{font-size:10px;fill:#b0bdcd}</style>',
             '<text x="20" y="27" class="title">2016 spacecraft grid cells — original polygon order</text>',
             '<text x="20" y="49" class="small">H=1 · D45=2 · V=4 · D135=8 · blue polygon, pink cursor, numbered vertices · Y increases upward</text>']
    probes = ((0.875, 0.5), (0.5, 0.875), (0.125, 0.5), (0.5, 0.125))
    for row, (mask, sectors) in enumerate(SHAPES.items()):
        y = 85 + row * 145
        families = "+".join(name for bit, name in ((1, "H"), (2, "45"), (4, "V"), (8, "135")) if mask & bit)
        lines.append(f'<text x="18" y="{y + 38}">0x{mask:X}</text>')
        lines.append(f'<text x="18" y="{y + 55}" class="small">{families}</text>')
        for sector, points in enumerate(sectors):
            ox, oy = 155 + sector * 210, y + 85
            def xy(px, py):
                return ox + px * 42, oy - py * 42
            lines.append(f'<text x="{ox - 15}" y="{y - 2}">{WEDGE_NAMES[sector]}</text>')
            lines.append(f'<path d="M {ox} {oy} h 42 v -42 h -42 Z" fill="none" stroke="#566278" stroke-dasharray="3 3"/>')
            coords = " ".join(f"{px},{py}" for px, py in (xy(vx / 2, vy / 2) for vx, vy in points))
            lines.append(f'<polygon points="{coords}" fill="#286b89" fill-opacity="0.7" stroke="#6bd5ed" stroke-width="2"/>')
            for index, (vx, vy) in enumerate(points):
                px, py = xy(vx / 2, vy / 2)
                lines.append(f'<text x="{px + 4}" y="{py - 4}" class="small">{index}</text>')
            px, py = xy(*probes[sector])
            lines.append(f'<circle cx="{px}" cy="{py}" r="3" fill="#ff92b5"/>')
        lines.append(f'<path d="M 15 {y + 122} H 935" stroke="#2c3544"/>')
    lines.append('</svg>')
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--self-test", action="store_true")
    group.add_argument("--table", action="store_true")
    group.add_argument("--atlas", action="store_true")
    group.add_argument("--point", nargs=4, metavar=("MASK", "X", "Y", "Z"),
                       help="mask (decimal or 0xHEX), then coordinates in feet")
    args = parser.parse_args()
    if args.self_test:
        result = self_test()
    elif args.table:
        result = table()
    elif args.atlas:
        print(atlas_svg())
        return
    else:
        result = fill_shape(int(args.point[0], 0), *(float(v) for v in args.point[1:]))
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
