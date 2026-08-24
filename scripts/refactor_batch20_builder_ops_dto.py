#!/usr/bin/env python3
"""Batch 20: Builder / ops / spatial DTO struct fields -> PascalCase."""

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

REPLACEMENTS = [
    # BoxSpec / SphereSpec declarations
    ("Point3d min{", "Point3d Min{"),
    ("Point3d max{", "Point3d Max{"),
    ("double tolerance{", "double Tolerance{"),
    ("Point3d center{", "Point3d Center{"),
    ("double radius{", "double Radius{"),
    ("int slices{", "int Slices{"),
    ("int stacks{", "int Stacks{"),
    # Profile2d / ExtrudeSpec
    ("std::vector<Point2d> outer", "std::vector<Point2d> Outer"),
    ("std::vector<std::vector<Point2d>> holes", "std::vector<std::vector<Point2d>> Holes"),
    ("Profile2d profile", "Profile2d Profile"),
    ("Plane plane{", "Plane Plane{"),
    ("double distance{", "double Distance{"),
    ("bool symmetric{", "bool Symmetric{"),
    ("std::string name{", "std::string Name{"),
    # PlanarPrismSpec (common fields)
    ("Point3d origin", "Point3d Origin"),
    ("Vector3d axis", "Vector3d Axis"),
    ("Vector3d u_axis", "Vector3d UAxis"),
    ("Vector3d v_axis", "Vector3d VAxis"),
    ("double height", "double Height"),
    # Access — spec.*
    ("spec.min", "spec.Min"),
    ("spec.max", "spec.Max"),
    ("spec.tolerance", "spec.Tolerance"),
    ("spec.center", "spec.Center"),
    ("spec.radius", "spec.Radius"),
    ("spec.slices", "spec.Slices"),
    ("spec.stacks", "spec.Stacks"),
    ("spec.profile", "spec.Profile"),
    ("spec.plane", "spec.Plane"),
    ("spec.distance", "spec.Distance"),
    ("spec.symmetric", "spec.Symmetric"),
    ("spec.name", "spec.Name"),
    ("s.min", "s.Min"),
    ("s.max", "s.Max"),
    ("s.tolerance", "s.Tolerance"),
    ("s.center", "s.Center"),
    ("s.radius", "s.Radius"),
    ("box.min", "box.Min"),
    ("box.max", "box.Max"),
    ("sphere.center", "sphere.Center"),
    ("sphere.radius", "sphere.Radius"),
    ("profile.outer", "profile.Outer"),
    ("profile.holes", "profile.Holes"),
    ("prism.origin", "prism.Origin"),
    ("prism.axis", "prism.Axis"),
    ("prism.u_axis", "prism.UAxis"),
    ("prism.v_axis", "prism.VAxis"),
    ("prism.height", "prism.Height"),
    # Aabb
    ("Aabb min", "Aabb Min"),
    ("Aabb max", "Aabb Max"),
    (".expand(o.min)", ".expand(o.Min)"),
    (".expand(o.max)", ".expand(o.Max)"),
]


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    if any(s in path.parts for s in SKIP_DIRS):
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
