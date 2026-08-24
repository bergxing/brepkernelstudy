#!/usr/bin/env python3
"""Fix incorrect header filename casing from fix_includes.py."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

REPLACEMENTS = {
    "Iobject.h": "IObject.h",
    "Objectregistry.h": "ObjectRegistry.h",
    "Featurehistory.h": "FeatureHistory.h",
    "Featuretree.h": "FeatureTree.h",
    "Booleanfeature.h": "BooleanFeature.h",
    "Boxfeature.h": "BoxFeature.h",
    "Spherefeature.h": "SphereFeature.h",
    "Sketchfeature.h": "SketchFeature.h",
    "Extrudefeature.h": "ExtrudeFeature.h",
    "LoopSample.h": "LoopSample.h",
    "SnapQuery.h": "SnapQuery.h",
    "XlDocument.h": "XlDocument.h",
    "BksCache.h": "BksCache.h",
}

ROOTS = [ROOT / "kernel", ROOT / "tests", ROOT / "apps" / "viewer", ROOT / "examples"]


def main() -> int:
    count = 0
    for base in ROOTS:
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if not path.is_file():
                continue
            if path.suffix.lower() not in {".cpp", ".h", ".hpp", ".cmake"}:
                if path.name not in {"CMakeLists.txt"}:
                    continue
            text = path.read_text(encoding="utf-8", errors="replace")
            new = text
            for old, fixed in REPLACEMENTS.items():
                new = new.replace(old, fixed)
            if new != text:
                path.write_text(new, encoding="utf-8", newline="\n")
                print(path.relative_to(ROOT))
                count += 1
    print(f"Updated {count} files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
