#!/usr/bin/env python3
"""Fix broken bool include casing from first fix_includes pass."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

REPLACEMENTS = {
    "Boxboolean.h": "BoxBoolean.h",
    "Boxrecognize.h": "BoxRecognize.h",
    "Fastpath.h": "FastPath.h",
    "Intersectplanecylinder.h": "IntersectPlaneCylinder.h",
    "Intersectplaneplane.h": "IntersectPlanePlane.h",
    "Intersectplanesphere.h": "IntersectPlaneSphere.h",
    "Intersectspherecylinder.h": "IntersectSphereCylinder.h",
    "Intersectspheresphere.h": "IntersectSphereSphere.h",
    "Intersectorregistry.h": "IntersectorRegistry.h",
    "Planarboolean.h": "PlanarBoolean.h",
    "Planarrecognize.h": "PlanarRecognize.h",
    "Sphereboxboolean.h": "SphereBoxBoolean.h",
    "Sphererecognize.h": "SphereRecognize.h",
    "Spheresphereboolean.h": "SphereSphereBoolean.h",
    "Compositeevaluator.h": "CompositeEvaluator.h",
    "Faceselector.h": "FaceSelector.h",
}

ROOTS = [ROOT / "kernel", ROOT / "tests", ROOT / "apps" / "viewer"]


def main() -> int:
    updated = 0
    for base in ROOTS:
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if not path.is_file() or path.suffix.lower() not in {".cpp", ".h", ".hpp"}:
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            new = text
            for old, new_name in REPLACEMENTS.items():
                new = new.replace(old, new_name)
            if new != text:
                path.write_text(new, encoding="utf-8", newline="\n")
                print(path.relative_to(ROOT))
                updated += 1
    print(f"Fixed {updated} files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
