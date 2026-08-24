#!/usr/bin/env python3
"""Batch 18: remaining kernel free functions -> PascalCase."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {
    "third_party",
    "cmake-build-mingw-debug",
    "build-kernel",
    "build-mingw",
    ".git",
    "build",
    "install-kernel",
}
SKIP_PATH_FRAGMENTS = (("apps", "viewer", "i18n"),)

REPLACEMENTS = [
    ("TriangulatePolygonWithHoles_unconstrained", "TriangulatePolygonWithHolesUnconstrained"),
    ("classify_point_in_prism", "ClassifyPointInPrism"),
    ("LastXl_error", "LastXlError"),
    ("extract_profile", "ExtractProfile"),
    ("ops::extrude(", "ops::Extrude("),
    ("brep::ops::extrude(", "brep::ops::Extrude("),
]


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    if any(s in path.parts for s in SKIP_DIRS):
        return False
    parts = path.parts
    for i in range(len(parts) - 2):
        if parts[i : i + 3] == SKIP_PATH_FRAGMENTS:
            return False
    return True


def main() -> None:
    changed = []
    for path in sorted(ROOT.rglob("*")):
        if not path.is_file() or not should_process(path):
            continue
        text = path.read_text(encoding="utf-8")
        updated = text
        for old, new in REPLACEMENTS:
            updated = updated.replace(old, new)
        if updated != text:
            path.write_text(updated, encoding="utf-8")
            changed.append(path.relative_to(ROOT))
    print(f"Patched {len(changed)} files")
    for p in changed:
        print(f"  {p}")


if __name__ == "__main__":
    main()
