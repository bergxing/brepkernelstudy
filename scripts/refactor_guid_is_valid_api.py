#!/usr/bin/env python3
"""Migrate brep::Guid::is_nil() call sites to IsValid() with inverted logic."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_PARTS = {"third_party", ".git", "cmake-build-mingw-debug", "install-kernel"}

NEGATED = re.compile(r"!([A-Za-z_][\w.]*)\.is_nil\(\)")
POSITIVE = re.compile(r"(?<!!)(?<![\w.])([A-Za-z_][\w.]*)\.is_nil\(\)")


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp", ".md", ".py"}:
        return False
    return not any(part in SKIP_PARTS for part in path.parts)


def transform(text: str) -> str:
    text = NEGATED.sub(r"\1.IsValid()", text)
    text = POSITIVE.sub(r"!\1.IsValid()", text)
    return text


def main() -> None:
    changed = 0
    for path in sorted(ROOT.rglob("*")):
        if not path.is_file() or not should_process(path):
            continue
        if path.name == "refactor_guid_is_valid_api.py":
            continue
        original = path.read_text(encoding="utf-8")
        if "is_nil()" not in original and "is_nil() const" not in original:
            continue
        updated = transform(original)
        if updated != original:
            path.write_text(updated, encoding="utf-8")
            print(f"updated {path.relative_to(ROOT)}")
            changed += 1
    print(f"done: {changed} file(s)")


if __name__ == "__main__":
    main()
