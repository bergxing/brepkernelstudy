#!/usr/bin/env python3
"""Batch 26d: remaining snake_case kernel sources -> PascalCase in git index."""

from __future__ import annotations

import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES = [
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


def git_mv_case_only(src: Path, dst: Path) -> None:
    rel = src.relative_to(ROOT)
    tracked = subprocess.run(
        ["git", "ls-files", "--error-unmatch", str(rel)],
        cwd=ROOT,
        capture_output=True,
    ).returncode == 0
    if not src.exists() and dst.exists():
        print(f"skip exists-only {dst.relative_to(ROOT)}")
        return
    if not src.exists():
        print(f"skip missing {rel}")
        return
    if src.name == dst.name:
        print(f"skip same {rel}")
        return
    if not tracked:
        if dst.exists():
            print(f"skip untracked {rel} (dest exists)")
            return
        dst.parent.mkdir(parents=True, exist_ok=True)
        src.rename(dst)
        subprocess.run(["git", "add", str(dst.relative_to(ROOT))], cwd=ROOT, check=True)
        print(f"added {dst.relative_to(ROOT)}")
        return
    tmp = src.with_name(f"__rename_tmp__{src.name}")
    if tmp.exists():
        tmp.unlink()
    subprocess.run(["git", "mv", str(src), str(tmp)], cwd=ROOT, check=True)
    subprocess.run(["git", "mv", str(tmp), str(dst)], cwd=ROOT, check=True)
    print(f"renamed {rel} -> {dst.relative_to(ROOT)}")


def main() -> None:
    for rel_src, rel_dst in RENAMES:
        git_mv_case_only(ROOT / rel_src, ROOT / rel_dst)


if __name__ == "__main__":
    main()
