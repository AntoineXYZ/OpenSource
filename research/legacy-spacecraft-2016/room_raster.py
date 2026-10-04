#!/usr/bin/env python3
"""Executable reference for 2016 DnFloorBuffer8 room painting.

Inputs and outputs here are integer quarter-grid raster coordinates relative to
the origin. The C++ adapter uses world feet. This models the recovered routines,
not a generic polygon Boolean operation; see PAINTING-RULES.txt.
"""

import argparse
import json
import math

from cell_shapes import SHAPES


STEP = 1.41421356225
SIZE = 516
ORIGIN = 256
CORNER = (
    (2, 0, 2, 1, 2, -1), (1, 1, 1, 2, 2, 1),
    (0, 2, -1, 2, 1, 2), (-1, 1, -2, 1, -1, 2),
    (-2, 0, -2, -1, -2, 1), (-1, -1, -1, -2, -2, -1),
    (0, -2, 1, -2, -1, -2), (1, -1, 2, -1, 1, -2),
)
CENTER = ((1, 1, 0, 1, 1, 0), (-1, 1, -1, 0, 0, 1),
          (-1, -1, 0, -1, -1, 0), (1, -1, 1, 0, 0, -1))
DIRECTIONS = ((4, 0), (2, 2), (0, 4), (-2, 2),
              (-4, 0), (-2, -2), (0, -4), (2, -2))


def on_segments(poly, point):
    """Original open-array segment test, evaluated in world feet."""
    px, py = (v * STEP for v in point)
    for a, b in zip(poly, poly[1:]):
        ax, ay = (v * STEP for v in a)
        bx, by = (v * STEP for v in b)
        dx, dy = bx - ax, by - ay
        length = math.sqrt(dx * dx + dy * dy)
        cx, cy = ax, ay
        if length:
            distance = ((px - ax) * dx + (py - ay) * dy) / length
            if distance > 0:
                if distance < length:
                    cx, cy = dx * distance / length + ax, dy * distance / length + ay
                else:
                    cx, cy = bx, by
        if math.sqrt((px - cx) ** 2 + (py - cy) ** 2) <= 1e-5:
            return True
    return False


class Raster:
    def __init__(self):
        self.samples = bytearray(SIZE * SIZE)

    def at(self, x, y):
        if not (0 <= x < SIZE and 0 <= y < SIZE):
            return 0
        return self.samples[x * SIZE + y]

    def put(self, x, y, value):
        assert 0 <= x < SIZE and 0 <= y < SIZE
        self.samples[x * SIZE + y] = value

    def perimeter(self, polygon, visit):
        assert len(polygon) >= 3
        points = [(x + ORIGIN, y + ORIGIN) for x, y in polygon]
        assert all(2 <= x <= 513 and 2 <= y <= 513 for x, y in points)
        x, y = points[-1]
        for ex, ey in points:
            dx, dy = ex - x, ey - y
            if dx and dy and abs(dx) != abs(dy):
                raise ValueError("edge outside the legacy grid")
            sx, sy = (dx > 0) - (dx < 0), (dy > 0) - (dy < 0)
            while (x, y) != (ex, ey):
                visit(x, y)
                x, y = x + sx, y + sy
        return (min(p[0] for p in points), max(p[0] for p in points),
                min(p[1] for p in points), max(p[1] for p in points))

    def fill(self, bounds, marker, erase, visit):
        xmin, xmax, ymin, ymax = bounds
        boundary = (lambda v: v == 0x56) if erase else (lambda v: v >= marker)
        continuation = (lambda v: v in (0x56, 9)) if erase else boundary
        for y in range(ymin, ymax + 1):
            start, consecutive = 9999, False
            for x in range(xmin, xmax + 1):
                if not boundary(self.at(x, y)):
                    if consecutive:
                        start = 9999 if x == start + 2 or not continuation(self.at(x + 1, y - 1)) else x - 1
                        consecutive = False
                    continue
                if (x % 4, y % 4) in ((0, 0), (2, 2)):
                    nw = boundary(self.at(x - 1, y + 1))
                    n = boundary(self.at(x, y + 1))
                    ne = boundary(self.at(x + 1, y + 1))
                    e = boundary(self.at(x + 1, y))
                    if (nw and n) or (n and ne) or (nw and ne) or not (nw or n or ne or e):
                        continue
                if start == 9999:
                    start = x
                elif x == start + 1:
                    consecutive = True
                elif not consecutive:
                    for inside in range(start + 1, x):
                        visit(inside, y)
                    start = 9999

    def add(self, polygon, marker):
        counts = [0, 0]

        def visit(x, y):
            sample = self.at(x, y)
            if sample < marker:
                sample += marker
                self.put(x, y, sample)
            if sample == 2:
                counts[0] += 1
            elif sample == 3:
                counts[1] += 1

        bounds = self.perimeter(polygon, visit)
        self.fill(bounds, marker, False, visit)
        return counts[0] != 0 and counts[1] >= 2

    def zero(self, polygon):
        removed = 0

        def perimeter(x, y):
            nonlocal removed
            sample = self.at(x, y)
            if sample != 0x56:
                removed += bool(sample)
                self.put(x, y, 0x56)

        def interior(x, y):
            nonlocal removed
            sample = self.at(x, y)
            if sample not in (9, 0x56):
                removed += bool(sample)
                self.put(x, y, 9)

        bounds = self.perimeter(polygon, perimeter)
        self.fill(bounds, 0, True, interior)
        xmin, xmax, ymin, ymax = bounds
        for y in range(ymin, ymax + 1):
            for x in range(xmin, xmax + 1):
                if self.at(x, y) == 9:
                    self.put(x, y, 0)
        for y in range(ymin, ymax + 1):
            for x in range(xmin, xmax + 1):
                if self.at(x, y) != 0x56:
                    continue
                self.put(x, y, 1)
                rx, ry = x % 4, y % 4
                bx, by = x - rx, y - ry
                supported = True
                if rx == 0:
                    if ry == 0:
                        supported = any(self.at(x + dx, y + dy) for dx, dy in
                                        ((2, 1), (1, 2), (-1, 2), (-2, 1),
                                         (-2, -1), (-1, -2), (1, -2), (2, -1)))
                    else:
                        supported = self.at(bx - 1, by + 2) or self.at(bx + 1, by + 2)
                elif rx == 2 and ry == 2:
                    supported = any(self.at(x + dx, y + dy) for dx, dy in ((1, 0), (0, 1), (-1, 0), (0, -1)))
                elif ry == 0:
                    supported = self.at(bx + 2, by - 1) or self.at(bx + 2, by + 1)
                elif (rx, ry) == (1, 1):
                    supported = self.at(bx + 2, by + 1) or self.at(bx + 1, by + 2)
                elif (rx, ry) == (1, 3):
                    supported = self.at(bx + 1, by + 2) or self.at(bx + 2, by + 3)
                elif (rx, ry) == (3, 3):
                    supported = self.at(bx + 3, by + 2) or self.at(bx + 2, by + 3)
                elif (rx, ry) == (3, 1):
                    supported = self.at(bx + 2, by + 1) or self.at(bx + 3, by + 2)
                if supported:
                    removed -= 1
                else:
                    self.put(x, y, 0)
        return removed != 0

    def candidate(self, x, y, d):
        mx, my, lx, ly, rx, ry = CORNER[d]
        if not self.at(x + mx, y + my) or self.at(x + lx, y + ly):
            return -1
        return d if self.at(x + rx, y + ry) else -2

    def corner(self, x, y, incoming=-1):
        if not self.at(x, y):
            return -1
        if incoming >= 0:
            for i in range(7):
                found = self.candidate(x, y, (incoming + 3 - i) % 8)
                if found != -1:
                    return found
            return -1
        found = -1
        for d in range(8):
            result = self.candidate(x, y, d)
            if result == -2 or (result >= 0 and found >= 0):
                return -2
            if result >= 0:
                found = result
        return found

    def center(self, x, y):
        if not self.at(x, y):
            return -1
        found = -1
        for i, (mx, my, lx, ly, rx, ry) in enumerate(CENTER):
            if not self.at(x + mx, y + my) or self.at(x + lx, y + ly):
                continue
            if not self.at(x + rx, y + ry) or found >= 0:
                return -2
            found = i * 2 + 1
        return found

    def chase(self, require_all_corners):
        xmin, ymin, xmax, ymax, count = 515, 515, 0, 0, 0
        for y in range(0, 515, 4):
            for x in range(0, 515, 4):
                if self.at(x, y):
                    xmin, xmax = min(xmin, x), max(xmax, x)
                    ymin, ymax = min(ymin, y), max(ymax, y)
                    edge = self.corner(x, y)
                    if edge == -2:
                        return None
                    count += edge != -1
        if xmax == 0:
            return []
        first, corner = -1, False
        for y in range(ymin, ymax + 1, 4):
            for x in range(xmin, xmax + 1, 4):
                if not self.at(x, y):
                    continue
                first = self.corner(x, y)
                if first == -2:
                    return None
                if first >= 0:
                    corner, start = True, (x, y)
                    break
                first = self.center(x, y)
                if first == -2:
                    return None
                if first >= 0:
                    start = (x + 2, y + 2)
                    break
            if first >= 0:
                break
        if first < 0:
            return []
        x, y = start
        result = [(x - ORIGIN, y - ORIGIN)]
        visited, direction = 0, first
        for remaining in range(10000, 0, -1):
            dx, dy = DIRECTIONS[direction]
            x, y = x + dx, y + dy
            if dx and dy:
                corner = not corner
            if corner:
                visited += 1
                nxt = self.corner(x, y, direction)
            else:
                nxt = self.center(x, y)
            if nxt < 0:
                return None
            point = (x - ORIGIN, y - ORIGIN)
            closed = (x, y) == start
            if not closed and on_segments(result, point):
                return None
            if nxt != direction:
                result.append(point)
            if closed or remaining == 1:
                if require_all_corners and count != visited:
                    return None
                if nxt == first:
                    result.pop(0)
                return result if len(result) >= 3 else None
            direction = nxt
        return None


def operation(boundary, cell, mode):
    raster = Raster()
    raster.add(boundary, 1)
    if mode == "merge":
        if not raster.add(cell, 2):
            return False, boundary
        result = raster.chase(False)
    else:
        if not raster.zero(cell):
            return mode == "can_subtract", boundary
        result = raster.chase(True)
        if mode == "can_subtract":
            return result is not None, boundary
    if result is None or result == boundary:
        return False, boundary
    return True, result


def inside_or_boundary(poly, p):
    """Independent exact-integer ray-crossing oracle for raster samples."""
    inside = False
    px, py = p
    for (ax, ay), (bx, by) in zip(poly, poly[1:] + poly[:1]):
        cross = (px - ax) * (by - ay) - (py - ay) * (bx - ax)
        if cross == 0 and min(ax, bx) <= px <= max(ax, bx) and min(ay, by) <= py <= max(ay, by):
            return True
        if (ay > py) != (by > py):
            if (cross < 0) == (by > ay):
                inside = not inside
    return inside


def square(x, y, w=4, h=4):
    return [(x, y), (x, y + h), (x + w, y + h), (x + w, y)]


def fixtures():
    square0 = square(0, 0)
    definitions = [
        ("same", "merge", square0, square0, False),
        ("adjacent", "merge", square0, square(4, 0), True),
        ("disconnected", "merge", square0, square(8, 0), False),
        ("corner_contact", "merge", square0, square(4, 4), False),
        ("erase_all", "subtract", square0, square0, True),
        ("erase_edge", "subtract", square(0, 0, 8, 4), square0, True),
        ("erase_outside", "subtract", square0, square(8, 0), False),
        ("erase_shared_edge", "subtract", square0, square(4, 0), False),
        ("erase_bridge", "can_subtract", square(0, 0, 12, 4), square(4, 0), False),
        ("erase_hole", "can_subtract", square(0, 0, 12, 12), square(4, 4), False),
        ("erase_triangle", "subtract", square0, [(0, 0), (4, 4), (4, 0)], True),
        ("merge_triangle", "merge", [(0, 0), (0, 4), (4, 4)], [(0, 0), (4, 4), (4, 0)], True),
        ("close_loop_fills_void", "merge",
         [(0, 0), (0, 12), (4, 12), (4, 4), (8, 4), (8, 12), (12, 12), (12, 0)],
         square(4, 8), True),
    ]
    result = []
    for name, mode, old, cell, expected in definitions:
        accepted, boundary = operation(old, cell, mode)
        assert accepted == expected, (name, accepted, boundary)
        result.append(dict(name=name, mode=mode, old=old, cell=cell,
                           accepted=accepted, boundary=boundary))
    return result


def self_test():
    checks = 0
    shapes = [[(x * 2, y * 2) for x, y in shape]
              for variants in SHAPES.values() for shape in variants]
    for poly in shapes:
        for ox, oy in ((0, 0), (-12, -8), (8, -12), (240 - 8, 240 - 8), (-240 + 8, -240 + 8)):
            translated = [(x + ox, y + oy) for x, y in poly]
            raster = Raster()
            raster.add(translated, 1)
            for x in range(min(p[0] for p in translated) - 1, max(p[0] for p in translated) + 2):
                for y in range(min(p[1] for p in translated) - 1, max(p[1] for p in translated) + 2):
                    assert bool(raster.at(x + ORIGIN, y + ORIGIN)) == inside_or_boundary(translated, (x, y)), (translated, x, y)
                    checks += 1
            traced = raster.chase(True)
            assert traced is not None, translated
            for x in range(min(p[0] for p in translated), max(p[0] for p in translated) + 1):
                for y in range(min(p[1] for p in translated), max(p[1] for p in translated) + 1):
                    assert inside_or_boundary(traced, (x, y)) == inside_or_boundary(translated, (x, y))
                    checks += 1
            erased = raster.zero(translated)
            assert erased and raster.chase(True) == [] and not any(raster.samples)
    # Interior samples of the four smallest triangles never lie on any enabled
    # grid line. Their occupancy independently checks subtraction without
    # reproducing ZeroBoundary's perimeter-repair predicates.
    pair_checks = 0
    unique = [list(poly) for poly in dict.fromkeys(tuple(poly) for poly in shapes)]
    for old in unique:
        for new in unique:
            for ox, oy in ((0, 0), (4, 0), (-4, 4)):
                cell = [(x + ox, y + oy) for x, y in new]
                raster = Raster()
                raster.add(old, 1)
                raster.zero(cell)
                for x in range(-5, 14):
                    for y in range(-5, 14):
                        if (x % 4, y % 4) not in ((1, 2), (2, 1), (2, 3), (3, 2)):
                            continue
                        expected = inside_or_boundary(old, (x, y)) and not inside_or_boundary(cell, (x, y))
                        assert bool(raster.at(x + ORIGIN, y + ORIGIN)) == expected, (old, cell, x, y)
                        pair_checks += 1
    examples = fixtures()
    # All sixteen combinations of the crossing predicate, checked against the
    # original decompiler branch structure rather than the simplified formula.
    for bits in range(16):
        nw, n, ne, e = (bool(bits & (1 << i)) for i in range(4))
        original = False
        if ((not nw) or (not n)) and ((not n) or (not ne)):
            if nw:
                if not ne:
                    original = True
            elif n or ne or e:
                original = True
        compact = not ((nw and n) or (n and ne) or (nw and ne) or not (nw or n or ne or e))
        assert original == compact
    print(f"PASS: {checks} independent occupancy/trace checks, {pair_checks} pairwise subtraction samples, 220 full erasures, {len(examples)} topology fixtures, 16 crossing predicates")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--fixtures", action="store_true")
    args = parser.parse_args()
    if args.fixtures:
        print(json.dumps({"coordinate_unit": "quarter-grid", "spacing": STEP,
                          "provenance": "static 2016 raster reconstruction, not old-client execution",
                          "cases": fixtures()}, indent=2))
    else:
        self_test()
