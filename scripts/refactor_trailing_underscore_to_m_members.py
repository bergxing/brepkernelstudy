#!/usr/bin/env python3
"""Rename class trailing-underscore members (foo_) to m_camelCase (m_foo).

Scans header member declarations, then applies boundary-safe renames in .h/.cpp.
Skips third_party and cmake-build-*.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SCAN_ROOTS = [
    ROOT / "kernel",
    ROOT / "apps" / "viewer",
    ROOT / "tests",
    ROOT / "examples",
]

SKIP_PARTS = {"third_party", ".git"}
MEMBER_DECL_RE = re.compile(
    r"(?<![a-zA-Z0-9_])"
    r"([a-zA-Z][a-zA-Z0-9]*(?:_[a-zA-Z0-9]+)*_)"
    r"(?=\s*(?:\{|=|;|\())"
)

# Never rename these identifiers (namespaces, etc.).
SKIP_NAMES = frozenset({"asm_"})


def should_skip(path: Path) -> bool:
    parts = {p.lower() for p in path.parts}
    if parts & SKIP_PARTS:
        return True
    return any(p.startswith("cmake-build") for p in path.parts)


def snake_to_m_camel(name: str) -> str:
    """Convert trailing-underscore member name to m_camelCase."""
    assert name.endswith("_")
    base = name[:-1]
    parts = base.split("_")
    if len(parts) == 1:
        word = parts[0]
        if not word:
            return name
        return "m_" + word[0].lower() + word[1:]
    first = parts[0].lower()
    tail = "".join(p[:1].upper() + p[1:] if p else "" for p in parts[1:])
    return "m_" + first + tail


def collect_members_from_sources() -> dict[str, str]:
    mapping: dict[str, str] = {}
    for base in SCAN_ROOTS:
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if path.suffix.lower() not in {".h", ".hpp", ".cpp"}:
                continue
            if should_skip(path):
                continue
            text = path.read_text(encoding="utf-8")
            for match in MEMBER_DECL_RE.finditer(text):
                old = match.group(1)
                if old in SKIP_NAMES:
                    continue
                new = snake_to_m_camel(old)
                if old != new:
                    mapping[old] = new
    return mapping


def replace_identifier(text: str, old: str, new: str) -> str:
    pattern = rf"(?<![a-zA-Z0-9_]){re.escape(old)}(?![a-zA-Z0-9_])"
    return re.sub(pattern, new, text)


def process_file(path: Path, mapping: dict[str, str]) -> bool:
    original = path.read_text(encoding="utf-8")
    updated = original
    for old, new in sorted(mapping.items(), key=lambda kv: len(kv[0]), reverse=True):
        updated = replace_identifier(updated, old, new)
    if updated != original:
        path.write_text(updated, encoding="utf-8")
        print(f"updated {path.relative_to(ROOT)}")
        return True
    return False


def main() -> int:
    mapping = collect_members_from_sources()
    if not mapping:
        print("no trailing-underscore members found in headers")
        return 0

    print(f"renaming {len(mapping)} member(s):")
    for old, new in sorted(mapping.items()):
        print(f"  {old} -> {new}")

    changed = 0
    for base in SCAN_ROOTS:
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if path.suffix.lower() not in {".h", ".hpp", ".cpp"}:
                continue
            if should_skip(path):
                continue
            if process_file(path, mapping):
                changed += 1

    print(f"done: {changed} file(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
