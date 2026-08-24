#!/usr/bin/env python3
"""Restore STL .size()/std::to_string; map Loop::Size to CoedgeCount."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TARGETS = [ROOT / "kernel" / "src", ROOT / "kernel" / "include"]


def fix_text(text: str) -> str:
    text = text.replace("loop->Size()", "loop->CoedgeCount()")
    text = text.replace("loop.Size()", "loop.CoedgeCount()")
    text = text.replace(".Size()", ".size()")
    text = text.replace("std::ToString", "std::to_string")
    return text


def main() -> None:
    changed = 0
    for base in TARGETS:
        for path in base.rglob("*"):
            if path.suffix.lower() not in {".cpp", ".h"}:
                continue
            original = path.read_text(encoding="utf-8")
            updated = fix_text(original)
            if updated != original:
                path.write_text(updated, encoding="utf-8")
                print(path.relative_to(ROOT))
                changed += 1
    print(f"done: {changed} files")


if __name__ == "__main__":
    main()
