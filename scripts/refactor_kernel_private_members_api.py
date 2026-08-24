#!/usr/bin/env python3
"""Rename kernel private trailing-underscore members to m_camelCase.

Only touches kernel/ sources. Uses boundary-safe regex so parameters like
origin_a are not corrupted.
"""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
KERNEL = ROOT / "kernel"

# Longer / compound names first.
REPLACEMENTS: list[tuple[str, str]] = [
    ("id_counter_", "m_idCounter"),
    ("face_bounds_", "m_faceBounds"),
    ("root_bounds_", "m_rootBounds"),
    ("next_entity_", "m_nextEntity"),
    ("next_constraint_", "m_nextConstraint"),
    ("direction_", "m_direction"),
    ("constraints_", "m_constraints"),
    ("occurrences_", "m_occurrences"),
    ("vertices_", "m_vertices"),
    ("coedges_", "m_coedges"),
    ("surfaces_", "m_surfaces"),
    ("curves2d_", "m_curves2d"),
    ("points_", "m_points"),
    ("curves_", "m_curves"),
    ("bodies_", "m_bodies"),
    ("circles_", "m_circles"),
    ("loops_", "m_loops"),
    ("shells_", "m_shells"),
    ("edges_", "m_edges"),
    ("faces_", "m_faces"),
    ("nodes_", "m_nodes"),
    ("mates_", "m_mates"),
    ("normal_", "m_normal"),
    ("radius_", "m_radius"),
    ("length_", "m_length"),
    ("center_", "m_center"),
    ("origin_", "m_origin"),
    ("u_axis_", "m_uAxis"),
    ("v_axis_", "m_vAxis"),
    ("x_axis_", "m_xAxis"),
    ("y_axis_", "m_yAxis"),
    ("axis_", "m_axis"),
    ("frame_", "m_frame"),
    ("lines_", "m_lines"),
    ("map_", "m_map"),
    ("xyz_", "m_xyz"),
    ("bytes_", "m_bytes"),
    ("root_", "m_root"),
    ("a_", "m_a"),
    ("b_", "m_b"),
]


def replace_member(text: str, old: str, new: str) -> str:
    pattern = rf"(?<![a-zA-Z0-9]){re.escape(old)}(?![a-zA-Z0-9])"
    return re.sub(pattern, new, text)


def process_file(path: Path) -> bool:
    original = path.read_text(encoding="utf-8")
    updated = original
    for old, new in REPLACEMENTS:
        updated = replace_member(updated, old, new)
    if updated != original:
        path.write_text(updated, encoding="utf-8")
        print(f"updated {path.relative_to(ROOT)}")
        return True
    return False


if __name__ == "__main__":
    changed = 0
    for path in sorted(KERNEL.rglob("*")):
        if path.is_file() and path.suffix in {".h", ".hpp", ".cpp"}:
            if process_file(path):
                changed += 1
    print(f"done: {changed} file(s)")
