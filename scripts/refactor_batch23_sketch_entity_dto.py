#!/usr/bin/env python3
"""Batch 23: mesh sampling + sketch entity struct fields -> PascalCase."""

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

LITERAL_REPLACEMENTS = [
    # LoopSample.h declarations
    ("Point3d xyz;", "Point3d Xyz;"),
    ("LoopType type{", "LoopType Type{"),
    ("std::vector<SampledPoint> points", "std::vector<SampledPoint> Points"),
    ("SampledRing outer", "SampledRing Outer"),
    ("std::vector<SampledRing> holes", "std::vector<SampledRing> Holes"),
    # Sketch.h declarations
    ("Point2d p{};", "Point2d P{};"),
    ("bool fixed{", "bool Fixed{"),
    ("SketchEntityId p0{}", "SketchEntityId P0{}"),
    ("SketchEntityId p1{}", "SketchEntityId P1{}"),
    ("SketchEntityId center{}", "SketchEntityId Center{}"),
    # mesh access
    ("region.outer", "region.Outer"),
    ("region.holes", "region.Holes"),
    ("candidate.outer", "candidate.Outer"),
    ("ring.points", "ring.Points"),
    ("hole.points", "hole.Points"),
    ("points = ring.points", "points = ring.Points"),
    ("point.xyz", "point.Xyz"),
    ("push_back({xyz,", "push_back({Xyz,"),
    # sketch access
    ("return p.p;", "return p.P;"),
    ("pa->p.", "pa->P."),
    ("pb->p.", "pb->P."),
    ("pa->p =", "pa->P ="),
    ("pb->p =", "pb->P ="),
    ("pt->p", "pt->P"),
    ("!pa->fixed", "!pa->Fixed"),
    ("!pb->fixed", "!pb->Fixed"),
    ("!p.fixed", "!p.Fixed"),
    ("p.fixed", "p.Fixed"),
    ("pt.fixed", "pt.Fixed"),
    (".p0", ".P0"),
    (".p1", ".P1"),
    ("c.center", "c.Center"),
    ("ln.p0", "ln.P0"),
    ("ln.p1", "ln.P1"),
    ("pt.p =", "pt.P ="),
    ("pt.fixed =", "pt.Fixed ="),
    ("w.u8(pt.fixed", "w.u8(pt.Fixed"),
    ("w.u32(ln.p0", "w.u32(ln.P0"),
    ("w.u32(ln.p1", "w.u32(ln.P1"),
    ("w.u32(c.center", "w.u32(c.Center"),
]

SKIP_PATH_FRAGMENTS = (("apps", "viewer", "i18n"),)


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
        original = path.read_text(encoding="utf-8")
        updated = original
        for old, new in LITERAL_REPLACEMENTS:
            updated = updated.replace(old, new)
        if updated != original:
            path.write_text(updated, encoding="utf-8")
            changed.append(path.relative_to(ROOT))
    print(f"Patched {len(changed)} files")
    for p in changed:
        print(f"  {p}")


if __name__ == "__main__":
    main()
