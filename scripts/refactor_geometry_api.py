#!/usr/bin/env python3
"""Update Geometry call sites after PascalCase refactor."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {
    "third_party",
    "cmake-build-mingw-debug",
    "cmake-build-mingw-release",
    "build-kernel",
    "build-mingw",
    ".git",
    "build",
    "install-kernel",
}

METHODS = [
    ("param_of", "ParamOf"),
    ("set_length", "SetLength"),
    ("set_xyz", "SetXyz"),
    ("kind", "Kind"),
    ("eval", "Eval"),
    ("tangent", "Tangent"),
    ("domain", "Domain"),
    ("normal", "Normal"),
    ("length", "Length"),
    ("origin", "Origin"),
    ("direction", "Direction"),
    ("center", "Center"),
    ("radius", "Radius"),
    ("xyz", "Xyz"),
    ("u_axis", "UAxis"),
    ("v_axis", "VAxis"),
    ("x_axis", "XAxis"),
    ("y_axis", "YAxis"),
    ("axis", "Axis"),
]


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    if "apps" in path.parts:
        return False
    return not any(skip in path.parts for skip in SKIP_DIRS)


def replace_methods(text: str) -> str:
    for old, new in METHODS:
        text = re.sub(rf"(->|\.){re.escape(old)}\(", rf"\1{new}(", text)
    return text


def process_file(path: Path) -> bool:
    original = path.read_text(encoding="utf-8")
    updated = replace_methods(original)
    if updated != original:
        path.write_text(updated, encoding="utf-8")
        return True
    return False


def main() -> None:
    changed = 0
    for path in ROOT.rglob("*"):
        if path.is_file() and should_process(path) and process_file(path):
            changed += 1
            print(path.relative_to(ROOT))
    print(f"Updated {changed} files.")


if __name__ == "__main__":
    main()
