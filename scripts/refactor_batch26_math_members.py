#!/usr/bin/env python3
"""Batch 26c: Math.h Eigen storage data_ -> m_data."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MATH = ROOT / "kernel/include/brep/Math.h"


def main() -> None:
    text = MATH.read_text(encoding="utf-8")
    updated = text.replace("data_", "m_data")
    if updated != text:
        MATH.write_text(updated, encoding="utf-8")
        print(f"Patched {MATH.relative_to(ROOT)}")
    else:
        print("No changes")


if __name__ == "__main__":
    main()
