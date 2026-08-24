#!/usr/bin/env python3
"""Migrate brepkernelstudy sources to project C++ style file naming.

- Headers: PascalCase.h
- Sources: PascalCase.cpp
- Updates #include paths, CMake lists, and text references.

Run from repo root: python scripts/migrate_style.py [--dry-run]
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SCAN_ROOTS = [
    ROOT / "kernel" / "include",
    ROOT / "kernel" / "src",
    ROOT / "kernel" / "internal",
    ROOT / "apps" / "viewer",
    ROOT / "tests",
    ROOT / "examples",
    ROOT / "cmake",
    ROOT / "scripts",
]

SOURCE_EXTS = {".cpp", ".hpp", ".h", ".hh", ".cxx", ".cc", ".inl"}
TEXT_UPDATE_ROOTS = [
    ROOT / "kernel",
    ROOT / "apps" / "viewer",
    ROOT / "tests",
    ROOT / "examples",
    ROOT / "cmake",
    ROOT / "scripts",
    ROOT / "docs",
    ROOT / ".cursor",
]

SKIP_PATH_PARTS = {"third_party", "cmake-build-mingw-debug", ".git"}

TEXT_UPDATE_SUFFIXES = SOURCE_EXTS | {".cmake", ".md", ".mdc", ".txt", ".py"}


def should_skip(path: Path) -> bool:
    parts = {p.lower() for p in path.parts}
    return bool(parts & SKIP_PATH_PARTS) or any(
        p.startswith("cmake-build") for p in path.parts
    )


INCLUDE_RE = re.compile(
    r'(#\s*include\s*["<])([^">]+)([">])',
)


def stem_to_pascal(stem: str) -> str:
    if stem.startswith("i") and "_" not in stem and len(stem) > 1 and stem[1].islower():
        return "I" + stem[1:].capitalize()
    if "_" not in stem and stem[:1].isupper():
        return stem
    return "".join(part.capitalize() for part in stem.split("_") if part)


def new_basename(old_name: str) -> str | None:
    path = Path(old_name)
    ext = path.suffix.lower()
    if ext not in {".cpp", ".hpp", ".h"}:
        return None
    new_ext = ".h" if ext in {".hpp", ".h"} else ".cpp"
    new_stem = stem_to_pascal(path.stem)
    if new_stem == path.stem and new_ext == ext:
        return None
    return new_stem + new_ext


def collect_files() -> list[Path]:
    files: list[Path] = []
    for base in SCAN_ROOTS:
        if not base.is_dir():
            continue
        for p in base.rglob("*"):
            if not p.is_file() or p.suffix.lower() not in SOURCE_EXTS:
                continue
            if should_skip(p):
                continue
            files.append(p)
    return sorted(set(files))


def build_rename_map(files: list[Path]) -> dict[Path, Path]:
    mapping: dict[Path, Path] = {}
    for src in files:
        new_base = new_basename(src.name)
        if not new_base:
            continue
        dst = src.with_name(new_base)
        if dst == src:
            continue
        mapping[src] = dst
    return mapping


def posix_include_path(path: Path) -> str:
    """Build include path as used in #include \"...\" for kernel headers."""
    rel = path.relative_to(ROOT).as_posix()
    if rel.startswith("kernel/include/"):
        return rel[len("kernel/include/") :]
    if rel.startswith("apps/viewer/"):
        return rel[len("apps/viewer/") :]
    return rel


def apply_renames(mapping: dict[Path, Path], dry_run: bool) -> None:
    # Deepest paths first to avoid directory issues.
    for src in sorted(mapping.keys(), key=lambda p: len(p.parts), reverse=True):
        dst = mapping[src]
        if dst.exists() and dst != src:
            print(f"SKIP (target exists): {src} -> {dst}", file=sys.stderr)
            continue
        print(f"RENAME {src.relative_to(ROOT)} -> {dst.relative_to(ROOT)}")
        if not dry_run:
            dst.parent.mkdir(parents=True, exist_ok=True)
            src.rename(dst)


def build_replace_pairs(mapping: dict[Path, Path]) -> list[tuple[str, str]]:
    pairs: list[tuple[str, str]] = []
    seen: set[tuple[str, str]] = set()

    def add(old: str, new: str) -> None:
        if old == new or not old:
            return
        key = (old, new)
        if key in seen:
            return
        seen.add(key)
        pairs.append(key)

    for src, dst in mapping.items():
        old_name = src.name
        new_name = dst.name
        add(old_name, new_name)

        old_inc = posix_include_path(src)
        new_inc = posix_include_path(dst)
        add(old_inc, new_inc)

        # Quoted include forms used in docs / scripts.
        add(f'"{old_inc}"', f'"{new_inc}"')
        add(f"'{old_inc}'", f"'{new_inc}'")

    # Longest old paths first to avoid partial replacements.
    pairs.sort(key=lambda x: len(x[0]), reverse=True)
    return pairs


def update_text_file(path: Path, pairs: list[tuple[str, str]], dry_run: bool) -> bool:
    try:
        text = path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError):
        return False
    original = text
    for old, new in pairs:
        text = text.replace(old, new)
    if text != original:
        print(f"UPDATE {path.relative_to(ROOT)}")
        if not dry_run:
            path.write_text(text, encoding="utf-8", newline="\n")
        return True
    return False


def iter_text_files() -> list[Path]:
    out: list[Path] = []
    for base in TEXT_UPDATE_ROOTS:
        if base.is_file():
            if base.suffix.lower() in TEXT_UPDATE_SUFFIXES:
                out.append(base)
            continue
        if not base.is_dir():
            continue
        for p in base.rglob("*"):
            if not p.is_file():
                continue
            if should_skip(p):
                continue
            if p.suffix.lower() in TEXT_UPDATE_SUFFIXES:
                out.append(p)
            elif p.name in {"CMakeLists.txt", "AGENTS.md"}:
                out.append(p)
    return sorted(set(out))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    files = collect_files()
    mapping = build_rename_map(files)
    print(f"Found {len(mapping)} files to rename")
    apply_renames(mapping, args.dry_run)

    pairs = build_replace_pairs(mapping)
    updated = 0
    for path in iter_text_files():
        if update_text_file(path, pairs, args.dry_run):
            updated += 1
    print(f"Updated {updated} text files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
