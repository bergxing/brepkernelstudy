#!/usr/bin/env python3
"""Repair corruption introduced by format_allman_braces.py.

- Restores PascalCase sources from healthy snake_case siblings when possible.
- Fixes split `/*name*/`, `/*store*/`, `/*id*/`, and `override` tokens.
- Collapses runaway blank-line inflation.

Run from repo root: python scripts/repair_allman_corruption.py [--dry-run]
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SCAN_DIRS = [
    ROOT / "kernel" / "include",
    ROOT / "kernel" / "src",
    ROOT / "kernel" / "internal",
    ROOT / "apps" / "viewer",
    ROOT / "tests",
    ROOT / "examples",
]

SKIP_PARTS = {"third_party", ".git", "cmake-build-mingw-debug", "build-kernel", "build-mingw", "install-kernel"}

sys.path.insert(0, str(ROOT / "scripts"))
from migrate_style import new_basename  # noqa: E402


def should_skip(path: Path) -> bool:
    parts = {p.lower() for p in path.parts}
    return bool(parts & SKIP_PARTS) or any(p.startswith("cmake-build") for p in path.parts)


def blank_ratio(text: str) -> float:
    lines = text.splitlines()
    if not lines:
        return 0.0
    return sum(1 for ln in lines if not ln.strip()) / len(lines)


def looks_corrupted(text: str) -> bool:
    lines = text.splitlines()
    if len(lines) < 200:
        return False
    return blank_ratio(text) > 0.35


def repair_text(text: str) -> str:
    original = text

    # /*name*/ split as /* \n { \n ame*/
    text = re.sub(r"/\*\s*\n\s*\{\s*\n\s*ame\*/", "/*name*/", text)
    # /*store*/ split as /* \n { \n }ore*/
    text = re.sub(r"/\*\s*\n\s*\{\s*\n\s*\}ore\*/", "/*store*/", text)
    # /*id*/ split as /* \n { \n d*/
    text = re.sub(r"/\*\s*\n\s*\{\s*\n\s*d\*/", "/*id*/", text)

    # override split across lines/braces
    text = re.sub(
        r"const over\s*\n\s*\{\s*\n\s*ide\s*\{\s*return",
        "const override\n  {\n    return",
        text,
    )
    text = re.sub(
        r"const ov\s*\n\s*\{\s*\n\s*rride\s*\{",
        "const override\n  {",
        text,
    )
    # const split across brace (e.g. ") c\n{\n    nst {")
    text = re.sub(
        r"\)\s*c\s*\n\s*\{\s*\n\s*nst\s*\{",
        ") const\n{",
        text,
    )
    # const after opening brace (e.g. ")\n{\n    const {")
    text = re.sub(
        r"\)\s*\n\s*\{\s*\n\s*const\s*\{",
        ") const\n{",
        text,
    )

    # Collapse 3+ consecutive blank lines
    text = re.sub(r"\n{4,}", "\n\n\n", text)

    return text


def snake_for_pascal(path: Path) -> Path | None:
    """Return snake_case sibling for a PascalCase.cpp/.h if naming maps."""
    nb = new_basename(path.name)
    if not nb or nb == path.name:
        return None
    # PascalCase -> snake_case: reverse via lowercase first char segments
    stem = path.stem
    if stem[:1].isupper():
        import re as _re

        snake = _re.sub(r"(?<!^)(?=[A-Z])", "_", stem).lower()
        if path.name.endswith(".cpp"):
            candidate = path.with_name(f"{snake}.cpp")
        else:
            candidate = path.with_name(f"{snake}.h")
        if candidate.exists() and candidate != path:
            return candidate
    return None


def pascal_for_snake(path: Path) -> Path | None:
    nb = new_basename(path.name)
    if not nb or nb == path.name:
        return None
    candidate = path.with_name(nb)
    if candidate.exists() and candidate != path:
        return candidate
    return None


def collect_files() -> list[Path]:
    out: list[Path] = []
    for base in SCAN_DIRS:
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if should_skip(path):
                continue
            if path.suffix.lower() in {".cpp", ".h", ".hpp"}:
                out.append(path)
    return sorted(set(out))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    restored = 0
    repaired = 0
    skipped = 0

    # Pass 1: restore corrupted PascalCase from healthy snake_case siblings
    seen_pascal: set[Path] = set()
    for path in collect_files():
        if path.suffix != ".cpp":
            continue
        nb = new_basename(path.name)
        if not nb or nb == path.name:
            continue
        pascal = path.with_name(nb)
        if pascal in seen_pascal:
            continue
        if not pascal.exists():
            continue
        seen_pascal.add(pascal)

        pascal_text = pascal.read_text(encoding="utf-8", errors="replace")
        if not looks_corrupted(pascal_text):
            continue

        snake_text = path.read_text(encoding="utf-8", errors="replace")
        if looks_corrupted(snake_text):
            continue

        print(f"restore {pascal.relative_to(ROOT)} <- {path.relative_to(ROOT)}")
        if not args.dry_run:
            pascal.write_text(snake_text, encoding="utf-8", newline="\n")
        restored += 1

    # Pass 2: token repair on all project sources
    for path in collect_files():
        text = path.read_text(encoding="utf-8", errors="replace")
        fixed = repair_text(text)
        if fixed == text:
            skipped += 1
            continue
        print(f"repair {path.relative_to(ROOT)}")
        if not args.dry_run:
            path.write_text(fixed, encoding="utf-8", newline="\n")
        repaired += 1

    print(f"restored={restored} repaired={repaired} unchanged={skipped}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
