#!/usr/bin/env python3
"""Fix BoxSpec/SphereSpec designated initializers after batch 20."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {"third_party", "cmake-build-mingw-debug", ".git", "build"}

REPLACEMENTS = [
    (".min =", ".Min ="),
    (".max =", ".Max ="),
    (".center =", ".Center ="),
    (".radius =", ".Radius ="),
    (".tolerance =", ".Tolerance ="),
    ("m.min.x()", "m.Min.x()"),
    ("m.max.x()", "m.Max.x()"),
]


def main() -> None:
    changed = []
    for path in sorted(ROOT.rglob("*")):
        if path.suffix not in {".cpp", ".h"}:
            continue
        if any(s in path.parts for s in SKIP_DIRS):
            continue
        if "scripts" in path.parts:
            continue
        text = path.read_text(encoding="utf-8")
        updated = text
        for old, new in REPLACEMENTS:
            updated = updated.replace(old, new)
        if updated != text:
            path.write_text(updated, encoding="utf-8")
            changed.append(path.relative_to(ROOT))
    print(f"Patched {len(changed)} files")


if __name__ == "__main__":
    main()
