#!/usr/bin/env python3
"""Recover sources corrupted by format_allman_braces (blank-line explosion).

1. Collapse runs of blank lines in files with >30% empty lines.
2. Materialize missing snake_case .hpp from collapsed PascalCase .h orphans.
3. Optionally delete orphan .h duplicates that already have a tracked .hpp.

Run: python scripts/recover_corrupted_sources.py [--delete-orphan-h]
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INCLUDE_ROOT = ROOT / "kernel" / "include"

SCAN_ROOTS = [
    ROOT / "kernel",
    ROOT / "apps" / "viewer",
    ROOT / "tests",
    ROOT / "examples",
]

SKIP_PARTS = {"third_party", ".git", "cmake-build-mingw-debug"}
SOURCE_EXTS = {".cpp", ".h", ".hpp", ".hxx", ".cxx", ".cc"}

INCLUDE_RE = re.compile(r'(#\s*include\s*["<])([^">]+)([">])')


def stem_to_pascal(stem: str) -> str:
    if stem.startswith("i") and "_" not in stem and len(stem) > 1 and stem[1].islower():
        return "I" + stem[1:].capitalize()
    if "_" not in stem and stem[:1].isupper():
        return stem
    return "".join(part.capitalize() for part in stem.split("_") if part)


def pascal_to_snake(name: str) -> str:
    s = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", name)
    return s.lower()


def empty_ratio(text: str) -> float:
    lines = text.splitlines()
    if not lines:
        return 0.0
    empty = sum(1 for ln in lines if not ln.strip())
    return empty / len(lines)


def collapse_blank_lines(text: str) -> str:
    out: list[str] = []
    prev_blank = False
    for line in text.splitlines():
        blank = not line.strip()
        if blank:
            if prev_blank:
                continue
            prev_blank = True
            out.append("")
            continue
        prev_blank = False
        out.append(line.rstrip())
    return "\n".join(out).rstrip("\n")


def tracked_hpp_files() -> list[Path]:
    result = subprocess.run(
        ["git", "ls-files", "kernel/include"],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=True,
    )
    return [
        ROOT / line.strip()
        for line in result.stdout.splitlines()
        if line.strip().endswith(".hpp")
    ]


def is_tracked(rel_posix: str) -> bool:
    return (
        subprocess.run(
            ["git", "ls-files", "--error-unmatch", rel_posix],
            cwd=ROOT,
            capture_output=True,
        ).returncode
        == 0
    )


def recover_file(path: Path) -> bool:
    original = path.read_text(encoding="utf-8", errors="replace")
    if empty_ratio(original) <= 0.3:
        return False
    collapsed = collapse_blank_lines(original)
    if collapsed == original.rstrip("\n"):
        return False
    had_nl = original.endswith("\n")
    path.write_text(collapsed + ("\n" if had_nl else ""), encoding="utf-8")
    print(f"collapsed {path.relative_to(ROOT)}")
    return True


def orphan_h_to_hpp(path: Path, hpp_stems: set[str]) -> Path | None:
    rel = path.relative_to(INCLUDE_ROOT)
    snake = pascal_to_snake(path.stem)
    hpp_name = f"{snake}.hpp"
    hpp_path = path.parent / hpp_name
    if hpp_path.exists() and is_tracked(hpp_path.relative_to(ROOT).as_posix()):
        return None
    if snake in hpp_stems and hpp_path.exists():
        return None
    return hpp_path


def materialize_missing_hpp(delete_orphan_h: bool) -> int:
    hpp_stems = {p.stem for p in tracked_hpp_files()}
    created = 0
    for path in sorted(INCLUDE_ROOT.rglob("*.h")):
        rel = path.relative_to(ROOT).as_posix()
        if is_tracked(rel):
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        if empty_ratio(text) <= 0.3:
            continue
        hpp_path = orphan_h_to_hpp(path, hpp_stems)
        if hpp_path is None:
            continue
        collapsed = collapse_blank_lines(text)
        hpp_path.parent.mkdir(parents=True, exist_ok=True)
        hpp_path.write_text(collapsed + "\n", encoding="utf-8")
        print(f"created {hpp_path.relative_to(ROOT)}")
        created += 1
        if delete_orphan_h:
            path.unlink()
            print(f"deleted {rel}")
    return created


def delete_orphan_h_with_hpp() -> int:
    hpp_stems = {p.stem for p in tracked_hpp_files()}
    deleted = 0
    for path in sorted(INCLUDE_ROOT.rglob("*.h")):
        rel = path.relative_to(ROOT).as_posix()
        if is_tracked(rel):
            continue
        snake = pascal_to_snake(path.stem)
        if snake not in hpp_stems and path.stem.lower() not in hpp_stems:
            continue
        path.unlink()
        print(f"deleted {rel}")
        deleted += 1
    return deleted


def delete_corrupted_viewer_duplicates() -> int:
    """Remove untracked PascalCase viewer sources when snake_case tracked twin exists."""
    deleted = 0
    viewer = ROOT / "apps" / "viewer"
    tracked = {
        line.strip()
        for line in subprocess.run(
            ["git", "ls-files", "apps/viewer"],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=True,
        ).stdout.splitlines()
    }
    tracked_lower = {t.lower() for t in tracked}
    for path in viewer.rglob("*"):
        if path.suffix.lower() not in SOURCE_EXTS:
            continue
        rel = path.relative_to(ROOT).as_posix()
        if rel in tracked:
            continue
        if empty_ratio(path.read_text(encoding="utf-8", errors="replace")) <= 0.3:
            continue
        # PascalCase.cpp / .h without underscore — likely duplicate of snake_case.
        stem = path.stem
        if "_" in stem or stem[:1].islower():
            continue
        parent = str(path.parent.relative_to(viewer)).replace("\\", "/")
        snake = pascal_to_snake(stem)
        ext = ".hpp" if path.suffix.lower() in {".h", ".hpp"} else ".cpp"
        candidates = [
            f"apps/viewer/{parent}/{snake}{ext}".replace("/./", "/"),
            f"apps/viewer/{snake}{ext}",
        ]
        if parent != ".":
            candidates.append(f"apps/viewer/{parent}/{snake}{ext}")
        twin = next((c for c in candidates if c in tracked or c.lower() in tracked_lower), None)
        if twin:
            path.unlink()
            print(f"deleted duplicate {rel}")
            deleted += 1
    return deleted


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--delete-orphan-h", action="store_true")
    args = parser.parse_args()

    collapsed = 0
    for base in SCAN_ROOTS:
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if path.suffix.lower() not in SOURCE_EXTS:
                continue
            if any(part.lower() in SKIP_PARTS for part in path.parts):
                continue
            if recover_file(path):
                collapsed += 1

    created = materialize_missing_hpp(args.delete_orphan_h)
    deleted = 0
    if args.delete_orphan_h:
        deleted += delete_orphan_h_with_hpp()
    deleted += delete_corrupted_viewer_duplicates()

    print(
        f"done: collapsed {collapsed}, created {created} .hpp, deleted {deleted} orphan file(s)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
