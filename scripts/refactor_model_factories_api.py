#!/usr/bin/env python3
"""Update Model factory / wiring call sites after PascalCase refactor."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {"third_party", "cmake-build-mingw-debug", ".git", "build", "install-kernel"}

# Longest names first to avoid partial substitution.
METHODS = [
    ("make_sphere_surface", "MakeSphereSurface"),
    ("make_cylinder_surface", "MakeCylinderSurface"),
    ("attach_edge_to_vertices", "AttachEdgeToVertices"),
    ("make_line2d", "MakeLine2d"),
    ("make_point", "MakePoint"),
    ("make_circle", "MakeCircle"),
    ("make_vertex", "MakeVertex"),
    ("make_coedge", "MakeCoedge"),
    ("make_plane", "MakePlane"),
    ("make_shell", "MakeShell"),
    ("make_edge", "MakeEdge"),
    ("make_loop", "MakeLoop"),
    ("make_face", "MakeFace"),
    ("make_body", "MakeBody"),
    ("make_line", "MakeLine"),
    ("link_loop", "LinkLoop"),
    ("pair_partners", "PairPartners"),
    ("next_id", "NextId"),
]


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    return not any(skip in path.parts for skip in SKIP_DIRS)


def replace_methods(text: str) -> str:
    for old, new in METHODS:
        text = text.replace(f"Model::{old}", f"Model::{new}")
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
