#!/usr/bin/env python3
"""Batch 24: rename kernel source files to PascalCase (git mv, two-step on Windows)."""

from __future__ import annotations

import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES = [
    ("kernel/src/builder.cpp", "kernel/src/Builder.cpp"),
    ("kernel/src/document.cpp", "kernel/src/Document.cpp"),
    ("kernel/src/dump.cpp", "kernel/src/Dump.cpp"),
    ("kernel/src/geometry.cpp", "kernel/src/Geometry.cpp"),
    ("kernel/src/guid.cpp", "kernel/src/Guid.cpp"),
    ("kernel/src/log.cpp", "kernel/src/Log.cpp"),
    ("kernel/src/mesh.cpp", "kernel/src/Mesh.cpp"),
    ("kernel/src/model.cpp", "kernel/src/Model.cpp"),
    ("kernel/src/part.cpp", "kernel/src/Part.cpp"),
    ("kernel/src/topology.cpp", "kernel/src/Topology.cpp"),
    ("kernel/src/validate.cpp", "kernel/src/Validate.cpp"),
    ("kernel/src/asm/assembly.cpp", "kernel/src/asm/Assembly.cpp"),
    ("kernel/src/bool/broadphase.cpp", "kernel/src/bool/Broadphase.cpp"),
    ("kernel/src/bool/classify.cpp", "kernel/src/bool/Classify.cpp"),
    ("kernel/src/bool/pipeline.cpp", "kernel/src/bool/Pipeline.cpp"),
    ("kernel/src/feat/context.cpp", "kernel/src/feat/Context.cpp"),
    ("kernel/src/feat/regenerator.cpp", "kernel/src/feat/Regenerator.cpp"),
    ("kernel/src/mesh/cdt.cpp", "kernel/src/mesh/Cdt.cpp"),
    ("kernel/src/ops/extrude.cpp", "kernel/src/ops/Extrude.cpp"),
    ("kernel/src/param/parameter.cpp", "kernel/src/param/Parameter.cpp"),
    ("kernel/src/sketch/sketch.cpp", "kernel/src/sketch/Sketch.cpp"),
    ("kernel/src/solve2d/solver.cpp", "kernel/src/solve2d/Solver.cpp"),
]


def git_mv_case_only(src: Path, dst: Path) -> None:
    if not src.exists():
        print(f"skip missing {src}")
        return
    if src.name == dst.name:
        print(f"skip already {dst.name}")
        return
    tmp = src.with_name(f"__rename_tmp__{src.name}")
    if tmp.exists():
        tmp.unlink()
    subprocess.run(["git", "mv", str(src), str(tmp)], cwd=ROOT, check=True)
    subprocess.run(["git", "mv", str(tmp), str(dst)], cwd=ROOT, check=True)
    print(f"renamed {src.relative_to(ROOT)} -> {dst.relative_to(ROOT)}")


def main() -> None:
    for rel_src, rel_dst in RENAMES:
        git_mv_case_only(ROOT / rel_src, ROOT / rel_dst)


if __name__ == "__main__":
    main()
