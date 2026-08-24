#!/usr/bin/env python3
"""Update Model public accessor call sites (Bodies, RemoveBody)."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {"third_party", "cmake-build-mingw-debug", ".git", "build", "install-kernel"}


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    return not any(skip in path.parts for skip in SKIP_DIRS)


def process_file(path: Path) -> bool:
    original = path.read_text(encoding="utf-8")
    updated = original
    updated = updated.replace("Model::remove_body", "Model::RemoveBody")
    updated = re.sub(r"\bremove_body\s*\(", "RemoveBody(", updated)
    updated = re.sub(r"(\.|->)bodies\s*\(\s*\)", r"\1Bodies()", updated)
    if updated != original:
        path.write_text(updated, encoding="utf-8")
        return True
    return False


def main() -> None:
    changed = 0
    for path in ROOT.rglob("*"):
        if path.is_file() and should_process(path) and process_file(path):
            changed += 1
            print(path.relative_to(ROOT))
    print(f"Updated {changed} files.")


if __name__ == "__main__":
    main()
