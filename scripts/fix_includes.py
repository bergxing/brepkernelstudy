#!/usr/bin/env python3
"""Fix #include paths to PascalCase.h after migrate_style.py."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

ROOTS = [
    ROOT / "kernel",
    ROOT / "apps" / "viewer",
    ROOT / "tests",
    ROOT / "examples",
    ROOT / "cmake",
    ROOT / "docs",
    ROOT / ".cursor",
    ROOT / "scripts",
]

SKIP = {"third_party", "cmake-build-mingw-debug", ".git"}


def stem_to_pascal(stem: str) -> str:
    if stem.startswith("i") and "_" not in stem and len(stem) > 1 and stem[1].islower():
        return "I" + stem[1:].capitalize()
    if "_" not in stem and stem[:1].isupper():
        return stem
    return "".join(part.capitalize() for part in stem.split("_") if part)


def fix_include_path(path: str) -> str:
    if path.startswith("brep/internal/"):
        return path
    # Never rewrite third-party / system library includes.
    normalized = path.replace("\\", "/")
    external_prefixes = (
        "spdlog/",
        "boost/",
        "gtest/",
        "gmock/",
        "Eigen/",
    )
    if normalized.startswith("<"):
        normalized = normalized[1:]
    if any(normalized.startswith(p) for p in external_prefixes):
        return path
    p = Path(normalized)
    if p.suffix.lower() not in {".hpp", ".h"}:
        return path
    new_name = stem_to_pascal(p.stem) + ".h"
    return str(p.with_name(new_name)).replace("\\", "/")


INCLUDE_RE = re.compile(
    r'(#\s*include\s*["<])([^">]+)([">])',
)


def fix_file(path: Path) -> bool:
    text = path.read_text(encoding="utf-8", errors="replace")
    changed = False

    def repl(m: re.Match[str]) -> str:
        nonlocal changed
        prefix, inc, suffix = m.group(1), m.group(2), m.group(3)
        new_inc = fix_include_path(inc)
        if new_inc != inc:
            changed = True
        return f"{prefix}{new_inc}{suffix}"

    new_text = INCLUDE_RE.sub(repl, text)
    if changed:
        path.write_text(new_text, encoding="utf-8", newline="\n")
    return changed


def main() -> int:
    updated = 0
    for base in ROOTS:
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if not path.is_file():
                continue
            if any(s in {p.lower() for p in path.parts} for s in SKIP):
                continue
            if path.suffix.lower() not in {".cpp", ".h", ".hpp", ".cmake", ".md", ".mdc", ".py"}:
                if path.name not in {"CMakeLists.txt", "AGENTS.md"}:
                    continue
            if fix_file(path):
                print(path.relative_to(ROOT))
                updated += 1
    print(f"Fixed includes in {updated} files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
