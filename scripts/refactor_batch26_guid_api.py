#!/usr/bin/env python3
"""Batch 26a: Guid static/instance methods -> PascalCase."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {"third_party", "cmake-build-mingw-debug", "build-kernel", "build-mingw", ".git", "build", "install-kernel"}

REPLACEMENTS = [
    ("Guid::from_string", "Guid::FromString"),
    ("Guid::to_string", "Guid::ToString"),
    ("Guid::from_bytes", "Guid::FromBytes"),
    ("Guid::generate", "Guid::Generate"),
    ("Guid::nil", "Guid::Nil"),
    ("Guid::from_string:", "Guid::FromString:"),
    (".from_string(", ".FromString("),
    (".from_bytes(", ".FromBytes("),
    (".bytes()", ".Bytes()"),
]


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    return not any(s in path.parts for s in SKIP_DIRS)


def main() -> None:
    changed = []
    for path in sorted(ROOT.rglob("*")):
        if not path.is_file() or not should_process(path):
            continue
        text = path.read_text(encoding="utf-8")
        updated = text
        for old, new in REPLACEMENTS:
            updated = updated.replace(old, new)
        if updated != text:
            path.write_text(updated, encoding="utf-8")
            changed.append(path.relative_to(ROOT))
    print(f"Patched {len(changed)} files")
    for p in changed:
        print(f"  {p}")


if __name__ == "__main__":
    main()
