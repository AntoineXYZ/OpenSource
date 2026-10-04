#!/usr/bin/env python3
"""Compare the user-built native dump with the independent Python .ship reader."""

import argparse
import importlib.util
import json
from pathlib import Path
import subprocess
import sys


def load_inspector():
    path = Path(__file__).resolve().parents[1] / "inspect_ship.py"
    spec = importlib.util.spec_from_file_location("legacy_ship_inspector", path)
    module = importlib.util.module_from_spec(spec)
    sys.dont_write_bytecode = True
    spec.loader.exec_module(module)
    return module


def original_schema(parsed):
    """Ignore later semantic aliases, retaining every original wire field."""
    room_keys = {
        "stream_offset", "design_id", "room_id", "deck", "room_type",
        "offset_0x08_u8", "stored_vertex_count", "retained_by_legacy_reader",
        "ellipse_controls", "vertices", "stream_size",
    }
    detail_keys = {
        "stream_offset", "deck", "offset_0x01_u8", "bool_raw_u8", "detail_id",
        "design_id", "bool_normalized", "packed_xy", "grid_xy", "decoded_xyz",
        "offset_0x28_u16", "offset_0x2a_u16_raw", "offset_0x2a_u16_normalized",
    }
    return {
        "header": parsed["header"],
        "modules": parsed["modules"],
        "rooms": [{key: room[key] for key in room_keys} for room in parsed["rooms"]],
        "details": [{key: detail[key] for key in detail_keys} for detail in parsed["details"]],
        "stream": parsed["stream"],
    }


def difference(expected, actual, path="records"):
    if isinstance(expected, dict):
        if not isinstance(actual, dict) or set(expected) != set(actual):
            return f"{path}: object keys differ"
        for key in expected:
            found = difference(expected[key], actual[key], f"{path}.{key}")
            if found:
                return found
    elif isinstance(expected, list):
        if not isinstance(actual, list) or len(expected) != len(actual):
            return f"{path}: array lengths differ"
        for index, (left, right) in enumerate(zip(expected, actual)):
            found = difference(left, right, f"{path}[{index}]")
            if found:
                return found
    elif expected != actual or (
        isinstance(expected, bool) != isinstance(actual, bool)
    ):
        return f"{path}: Python={expected!r}, native={actual!r}"
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("native_executable", type=Path, help="already-built native_ship_reader executable")
    parser.add_argument("ships", nargs="+", type=Path)
    args = parser.parse_args()
    inspector = load_inspector()
    paths = [path.resolve() for path in args.ships]
    try:
        result = subprocess.run(
            [str(args.native_executable.resolve()), *map(str, paths)],
            capture_output=True, text=True, check=False,
        )
    except OSError as error:
        print(f"Cannot run the already-built native harness: {error}", file=sys.stderr)
        return 1
    if result.returncode:
        sys.stderr.write(result.stderr or result.stdout)
        return 1
    try:
        files = json.loads(result.stdout)["files"]
        if len(files) != len(paths):
            raise ValueError("native result count does not match the input count")
        for path, native in zip(paths, files):
            if native["path"] != str(path):
                raise ValueError(f"native path mismatch for {path}")
            with path.open("rb") as source:
                data = source.read(inspector.MAX_FILE_BYTES + 1)
            expected = original_schema(inspector.parse_ship(data))
            mismatch = difference(expected, native["records"])
            if mismatch:
                raise ValueError(f"{path.name}: {mismatch}")
            print(f"PASS: {path.name}: every decoded wire field, point and stream offset matches")
    except (KeyError, OSError, ValueError) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
