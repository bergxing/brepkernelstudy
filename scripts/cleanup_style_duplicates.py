#!/usr/bin/env python3
"""Remove obsolete snake_case / .hpp duplicates after PascalCase migration."""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

ROOTS = [
    ROOT / "kernel" / "include",
    ROOT / "kernel" / "src",
    ROOT / "kernel" / "internal",
    ROOT / "apps" / "viewer",
    ROOT / "tests",
    ROOT / "examples",
]


def stem_to_pascal(stem: str) -> str:
    if stem.startswith("i") and "_" not in stem and len(stem) > 1 and stem[1].islower():
        return "I" + stem[1:].capitalize()
    return "".join(part.capitalize() for part in stem.split("_") if part)


def canonical_name(path: Path) -> str:
    ext = ".h" if path.suffix.lower() in {".hpp", ".h"} else ".cpp"
    return stem_to_pascal(path.stem) + ext


def main() -> int:
    removed = 0
    for base in ROOTS:
        if not base.is_dir():
            continue
        by_dir: dict[Path, list[Path]] = {}
        for p in base.rglob("*"):
            if p.suffix.lower() not in {".cpp", ".hpp", ".h"}:
                continue
            by_dir.setdefault(p.parent, []).append(p)

        for directory, files in by_dir.items():
            canonical: dict[str, Path] = {}
            for p in files:
                key = canonical_name(p).lower()
                existing = canonical.get(key)
                if existing is None:
                    canonical[key] = p
                    continue
                # Prefer PascalCase + .h/.cpp over snake + .hpp
                def score(path: Path) -> tuple[int, int]:
                    name = path.name
                    pascal = int(name == canonical_name(path))
                    ext = int(path.suffix.lower() in {".h", ".cpp"})
                    return (pascal, ext)

                if score(p) > score(existing):
                    loser = existing
                    canonical[key] = p
                else:
                    loser = p
                print(f"REMOVE duplicate {loser.relative_to(ROOT)}")
                loser.unlink()
                removed += 1

    print(f"Removed {removed} duplicate files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
