#!/usr/bin/env python3
"""Rename Parameter struct fields: kind/value/user_driven -> Kind/Value/UserDriven."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {
    "third_party",
    "cmake-build-mingw-debug",
    "build-kernel",
    "build-mingw",
    ".git",
    "build",
    "install-kernel",
}

REPLACEMENTS = [
    ("param.user_driven", "param.UserDriven"),
    ("p.user_driven", "p.UserDriven"),
    ("param->user_driven", "param->UserDriven"),
    ("param.user_driven", "param.UserDriven"),
    ("param->kind", "param->Kind"),
    ("param->value", "param->Value"),
    ("param.kind", "param.Kind"),
    ("param.value", "param.Value"),
    ("p->value", "p->Value"),
    ("p.value", "p.Value"),
    ("p->kind", "p->Kind"),
    ("p.kind", "p.Kind"),
    ("->value", "->Value"),  # only after param/p - order matters, applied globally risky
]

# Safer targeted replacements only
SAFE_REPLACEMENTS = [
    ("param.user_driven", "param.UserDriven"),
    ("p.user_driven", "p.UserDriven"),
    ("param->user_driven", "param->UserDriven"),
    ("param->kind", "param->Kind"),
    ("param->value", "param->Value"),
    ("param.kind", "param.Kind"),
    ("param.value", "param.Value"),
    ("p->kind", "p->Kind"),
    ("p->value", "p->Value"),
    ("p.kind", "p.Kind"),
    ("p.value", "p.Value"),
    ("ParamKind kind{", "ParamKind Kind{"),
    ("double value{", "double Value{"),
    ("bool user_driven{", "bool UserDriven{"),
]


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    if any(skip in path.parts for skip in SKIP_DIRS):
        return False
    if "apps" in path.parts and "viewer" not in path.parts:
        return False
    parts = path.parts
    for i in range(len(parts) - 2):
        if parts[i : i + 3] == ("apps", "viewer", "i18n"):
            return False
    return True


def process_file(path: Path) -> bool:
    original = path.read_text(encoding="utf-8")
    updated = original
    for old, new in SAFE_REPLACEMENTS:
        updated = updated.replace(old, new)
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
