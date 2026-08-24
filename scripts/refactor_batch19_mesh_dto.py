#!/usr/bin/env python3
"""Batch 19: Mesh / CDT DTO struct fields -> PascalCase (regex-safe)."""

from __future__ import annotations

import re
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

LITERAL_REPLACEMENTS = [
    ("MeshVertex position", "MeshVertex Position"),
    ("MeshVertex normal", "MeshVertex Normal"),
    ("std::vector<MeshVertex> vertices", "std::vector<MeshVertex> Vertices"),
    ("std::vector<std::uint32_t> indices", "std::vector<std::uint32_t> Indices"),
    ("std::vector<Point3d> positions", "std::vector<Point3d> Positions"),
    ("CdtResult ok", "CdtResult Ok"),
    ("std::string diagnostics", "std::string Diagnostics"),
    ("std::vector<CdtVertex> vertices", "std::vector<CdtVertex> Vertices"),
    ("std::vector<CdtTriangle> triangles", "std::vector<CdtTriangle> Triangles"),
    ("Point2d uv{}", "Point2d Uv{}"),
    ("Point2d uv;", "Point2d Uv;"),
    ("Point2d uv{", "Point2d Uv{"),
    ("SampledPoint xyz", "SampledPoint Xyz"),
    ("int v0", "int V0"),
    ("int v1", "int V1"),
    ("int v2", "int V2"),
]

# Member access: .field not followed by identifier char (avoids .normalized()).
REGEX_REPLACEMENTS = [
    (re.compile(r"\.vertices\b"), ".Vertices"),
    (re.compile(r"\.indices\b"), ".Indices"),
    (re.compile(r"\.positions\b"), ".Positions"),
    (re.compile(r"\.position\b"), ".Position"),
    (re.compile(r"\.normal\b"), ".Normal"),
    (re.compile(r"\.uv\b"), ".Uv"),
    (re.compile(r"\.xyz\b"), ".Xyz"),
    (re.compile(r"\.diagnostics\b"), ".Diagnostics"),
    (re.compile(r"\.triangles\b"), ".Triangles"),
    (re.compile(r"\.v0\b"), ".V0"),
    (re.compile(r"\.v1\b"), ".V1"),
    (re.compile(r"\.v2\b"), ".V2"),
    (re.compile(r"\.ok\b"), ".Ok"),
]


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    if any(s in path.parts for s in SKIP_DIRS):
        return False
    return True


def patch(text: str) -> str:
    for old, new in LITERAL_REPLACEMENTS:
        text = text.replace(old, new)
    for pattern, repl in REGEX_REPLACEMENTS:
        text = pattern.sub(repl, text)
    return text


def main() -> None:
    changed = []
    for path in sorted(ROOT.rglob("*")):
        if not path.is_file() or not should_process(path):
            continue
        original = path.read_text(encoding="utf-8")
        updated = patch(original)
        if updated != original:
            path.write_text(updated, encoding="utf-8")
            changed.append(path.relative_to(ROOT))
    print(f"Patched {len(changed)} files")
    for p in changed:
        print(f"  {p}")


if __name__ == "__main__":
    main()
