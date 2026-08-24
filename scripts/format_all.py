#!/usr/bin/env python3
"""Run clang-format on project C++ sources (4-space Allman via .clang-format)."""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DIRS = [
    ROOT / "kernel" / "include",
    ROOT / "kernel" / "src",
    ROOT / "kernel" / "internal",
    ROOT / "apps" / "viewer",
    ROOT / "tests",
    ROOT / "examples",
]


def main() -> int:
    clang_format = shutil.which("clang-format")
    if not clang_format:
        print("clang-format not found in PATH; skip formatting", file=sys.stderr)
        return 0

    files: list[Path] = []
    for base in DIRS:
        if not base.is_dir():
            continue
        for p in base.rglob("*"):
            if p.suffix.lower() in {".cpp", ".h", ".hpp"}:
                if any(part.startswith("cmake-build") for part in p.parts):
                    continue
                files.append(p)

    if not files:
        return 0

    print(f"Formatting {len(files)} files...")
    subprocess.run([clang_format, "-i", *[str(f) for f in files]], check=False)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
