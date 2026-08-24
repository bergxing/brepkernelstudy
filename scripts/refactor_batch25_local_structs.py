#!/usr/bin/env python3
"""Batch 25: local/anonymous struct fields in kernel .cpp -> PascalCase."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "kernel" / "src"

REPLACEMENTS = [
    # Builder.cpp FaceBuild
    ("Point3d origin;", "Point3d Origin;"),
    ("Vector3d u_axis;", "Vector3d UAxis;"),
    ("Vector3d v_axis;", "Vector3d VAxis;"),
    ("std::array<int, 4> edge_idx;", "std::array<int, 4> EdgeIdx;"),
    ("std::array<bool, 4> forward;", "std::array<bool, 4> Forward;"),
    ("std::array<Point2d, 5> uv;", "std::array<Point2d, 5> Uv;"),
    (".origin", ".Origin"),
    (".u_axis", ".UAxis"),
    (".v_axis", ".VAxis"),
    (".edge_idx", ".EdgeIdx"),
    (".forward", ".Forward"),
    # RingGeom (Extrude.cpp)
    ("Vertex* bottom_v;", "Vertex* BottomV;"),
    ("Vertex* top_v;", "Vertex* TopV;"),
    ("Edge* bottom_e;", "Edge* BottomE;"),
    ("Edge* top_e;", "Edge* TopE;"),
    ("Edge* vert_e;", "Edge* VertE;"),
    ("bool is_hole;", "bool IsHole;"),
    (".bottom_v", ".BottomV"),
    (".top_v", ".TopV"),
    (".bottom_e", ".BottomE"),
    (".top_e", ".TopE"),
    (".vert_e", ".VertE"),
    (".is_hole", ".IsHole"),
    # ComplementMeshWelder
    ("double weld_tol;", "double WeldTol;"),
    (".weld_tol", ".WeldTol"),
]


def main() -> None:
    changed = []
    for path in sorted(ROOT.rglob("*.cpp")):
        text = path.read_text(encoding="utf-8")
        updated = text
        for old, new in REPLACEMENTS:
            updated = updated.replace(old, new)
        if updated != text:
            path.write_text(updated, encoding="utf-8")
            changed.append(path.relative_to(ROOT.parent.parent))
    print(f"Patched {len(changed)} files")
    for p in changed:
        print(f"  {p}")


if __name__ == "__main__":
    main()
