#!/usr/bin/env python3
"""Batch 22: spatial API (Aabb, FaceBvh, FaceBvhNode, QueryStats) -> PascalCase."""

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
    ("bool empty() const noexcept", "bool Empty() const noexcept"),
    ("bool overlaps(const Aabb&", "bool Overlaps(const Aabb&"),
    ("double surface_area() const noexcept", "double SurfaceArea() const noexcept"),
    ("Point3d center() const noexcept", "Point3d Center() const noexcept"),
    ("void expand(const Point3d&", "void Expand(const Point3d&"),
    ("void expand(const Aabb&", "void Expand(const Aabb&"),
    ("static Aabb merge(const Aabb&", "static Aabb Merge(const Aabb&"),
    ("int longest_axis() const noexcept", "int LongestAxis() const noexcept"),
    ("Aabb::merge(", "Aabb::Merge("),
    ("Aabb bounds;", "Aabb Bounds;"),
    ("int left{", "int Left{"),
    ("int right{", "int Right{"),
    ("int face_begin{", "int FaceBegin{"),
    ("bool is_leaf() const noexcept", "bool IsLeaf() const noexcept"),
    ("std::size_t nodes_visited{", "std::size_t NodesVisited{"),
    ("static constexpr int k_default_leaf_max", "static constexpr int kDefaultLeafMax"),
    ("static constexpr int k_sah_bins", "static constexpr int kSahBins"),
    ("static FaceBvh build(", "static FaceBvh Build("),
    ("query_overlaps(", "QueryOverlaps("),
    ("candidate_pairs(", "CandidatePairs("),
    ("root_bounds() const", "RootBounds() const"),
    ("bool empty() const noexcept { return m_faces.empty()", "bool Empty() const noexcept { return m_faces.empty()"),
    ("const std::vector<FaceBvhNode>& nodes() const", "const std::vector<FaceBvhNode>& Nodes() const"),
    ("FaceBvh::build(", "FaceBvh::Build("),
    ("FaceBvh::query_overlaps(", "FaceBvh::QueryOverlaps("),
    ("FaceBvh::candidate_pairs(", "FaceBvh::CandidatePairs("),
    ("int build_median(", "int BuildMedian("),
    ("int build_sah(", "int BuildSah("),
    ("void apply_order(", "void ApplyOrder("),
    ("void query_node(", "void QueryNode("),
    ("void visit_pairs(", "void VisitPairs("),
    ("Facebvh.h", "FaceBvh.h"),
    ("node.bounds", "node.Bounds"),
    ("node.face_begin", "node.FaceBegin"),
    ("node.left", "node.Left"),
    ("node.right", "node.Right"),
    ("A.bounds", "A.Bounds"),
    ("B.bounds", "B.Bounds"),
    ("A.face_begin", "A.FaceBegin"),
    ("B.face_begin", "B.FaceBegin"),
    ("A.left", "A.Left"),
    ("A.right", "A.Right"),
    ("B.left", "B.Left"),
    ("B.right", "B.Right"),
    ("++stats->nodes_visited", "++stats->NodesVisited"),
    ("med.candidate_pairs", "med.CandidatePairs"),
    ("sah.candidate_pairs", "sah.CandidatePairs"),
]

REGEX_REPLACEMENTS = [
    (re.compile(r"\.surface_area\(\)"), ".SurfaceArea()"),
    (re.compile(r"\.longest_axis\(\)"), ".LongestAxis()"),
    (re.compile(r"\.is_leaf\(\)"), ".IsLeaf()"),
    (re.compile(r"\.overlaps\("), ".Overlaps("),
    (re.compile(r"\.expand\("), ".Expand("),
    (re.compile(r"box\.center\(\)"), "box.Center()"),
    (re.compile(r"\bo\.empty\(\)"), "o.Empty()"),
]

SKIP_FILES = {
    "apps/viewer/ui/CursorTip.cpp",
    "apps/viewer/SelectRectOverlay.cpp",
}


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    if any(s in path.parts for s in SKIP_DIRS):
        return False
    rel = path.relative_to(ROOT).as_posix()
    if rel in SKIP_FILES:
        return False
    return True


def main() -> None:
    changed = []
    for path in sorted(ROOT.rglob("*")):
        if not path.is_file() or not should_process(path):
            continue
        original = path.read_text(encoding="utf-8")
        updated = original
        for old, new in LITERAL_REPLACEMENTS:
            updated = updated.replace(old, new)
        for pattern, repl in REGEX_REPLACEMENTS:
            updated = pattern.sub(repl, updated)
        if updated != original:
            path.write_text(updated, encoding="utf-8")
            changed.append(path.relative_to(ROOT))
    print(f"Patched {len(changed)} files")
    for p in changed:
        print(f"  {p}")


if __name__ == "__main__":
    main()
