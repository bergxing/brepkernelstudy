#!/usr/bin/env python3
"""Fix git index: drop snake_case paths, stage PascalCase counterparts."""

from __future__ import annotations

import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

PAIRS = [
    ("kernel/src/bool/box_boolean.cpp", "kernel/src/bool/BoxBoolean.cpp"),
    ("kernel/src/bool/box_recognize.cpp", "kernel/src/bool/BoxRecognize.cpp"),
    ("kernel/src/bool/evaluator_stub.cpp", "kernel/src/bool/EvaluatorStub.cpp"),
    ("kernel/src/bool/intersect_plane_cylinder.cpp", "kernel/src/bool/IntersectPlaneCylinder.cpp"),
    ("kernel/src/bool/intersect_plane_plane.cpp", "kernel/src/bool/IntersectPlanePlane.cpp"),
    ("kernel/src/bool/intersect_plane_sphere.cpp", "kernel/src/bool/IntersectPlaneSphere.cpp"),
    ("kernel/src/bool/intersect_sphere_cylinder.cpp", "kernel/src/bool/IntersectSphereCylinder.cpp"),
    ("kernel/src/bool/intersect_sphere_sphere.cpp", "kernel/src/bool/IntersectSphereSphere.cpp"),
    ("kernel/src/bool/planar_boolean.cpp", "kernel/src/bool/PlanarBoolean.cpp"),
    ("kernel/src/bool/planar_recognize.cpp", "kernel/src/bool/PlanarRecognize.cpp"),
    ("kernel/src/bool/sphere_box_boolean.cpp", "kernel/src/bool/SphereBoxBoolean.cpp"),
    ("kernel/src/bool/sphere_recognize.cpp", "kernel/src/bool/SphereRecognize.cpp"),
    ("kernel/src/bool/sphere_sphere_boolean.cpp", "kernel/src/bool/SphereSphereBoolean.cpp"),
    ("kernel/src/feat/boolean_feature.cpp", "kernel/src/feat/BooleanFeature.cpp"),
    ("kernel/src/feat/box_feature.cpp", "kernel/src/feat/BoxFeature.cpp"),
    ("kernel/src/feat/extrude_feature.cpp", "kernel/src/feat/ExtrudeFeature.cpp"),
    ("kernel/src/feat/feature_history.cpp", "kernel/src/feat/FeatureHistory.cpp"),
    ("kernel/src/feat/feature_tree.cpp", "kernel/src/feat/FeatureTree.cpp"),
    ("kernel/src/feat/sketch_feature.cpp", "kernel/src/feat/SketchFeature.cpp"),
    ("kernel/src/feat/sphere_feature.cpp", "kernel/src/feat/SphereFeature.cpp"),
    ("kernel/src/io/bks_cache.cpp", "kernel/src/io/BksCache.cpp"),
    ("kernel/src/io/xl_document.cpp", "kernel/src/io/XlDocument.cpp"),
    ("kernel/src/mesh/loop_sample.cpp", "kernel/src/mesh/LoopSample.cpp"),
    ("kernel/src/snap/snap_query.cpp", "kernel/src/snap/SnapQuery.cpp"),
    ("kernel/src/spatial/face_bvh.cpp", "kernel/src/spatial/FaceBvh.cpp"),
    ("kernel/src/object_registry.cpp", "kernel/src/ObjectRegistry.cpp"),
]


def tracked(rel: str) -> bool:
    return (
        subprocess.run(
            ["git", "ls-files", "--error-unmatch", rel],
            cwd=ROOT,
            capture_output=True,
        ).returncode
        == 0
    )


def main() -> None:
    for old, new in PAIRS:
        old_path = ROOT / old
        new_path = ROOT / new
        if not tracked(old):
            if new_path.exists() and not tracked(new):
                subprocess.run(["git", "add", new], cwd=ROOT, check=True)
                print(f"added untracked {new}")
            continue
        if tracked(new):
            subprocess.run(["git", "rm", "--cached", "-f", old], cwd=ROOT, check=True)
            print(f"removed stale index entry {old}")
            continue
        if new_path.exists():
            subprocess.run(["git", "rm", "--cached", "-f", old], cwd=ROOT, check=True)
            subprocess.run(["git", "add", new], cwd=ROOT, check=True)
            print(f"reindexed {old} -> {new}")
        else:
            subprocess.run(["git", "mv", old, new], cwd=ROOT, check=True)
            print(f"git mv {old} -> {new}")


if __name__ == "__main__":
    main()
