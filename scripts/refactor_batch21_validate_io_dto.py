#!/usr/bin/env python3
"""Batch 21: Validate + IO result DTO fields/methods -> PascalCase."""

from __future__ import annotations

import re
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

LITERAL_REPLACEMENTS = [
    ("Severity severity{", "Severity Severity{"),
    ("std::string where", "std::string Where"),
    ("std::string message", "std::string Message"),
    ("std::vector<ValidationIssue> issues", "std::vector<ValidationIssue> Issues"),
    ("bool ok() const", "bool Ok() const"),
    ("void error(std::string where", "void Error(std::string where"),
    ("void warning(std::string where", "void Warning(std::string where"),
    ("bool ok{", "bool Ok{"),
    ("std::string error", "std::string Error"),
    ("XlSaveResult ok", "XlSaveResult Ok"),
    ("XlLoadResult ok", "XlLoadResult Ok"),
    ("CacheSaveResult ok", "CacheSaveResult Ok"),
    ("CacheLoadResult ok", "CacheLoadResult Ok"),
    ("RegenResult ok", "RegenResult Ok"),
]

REGEX_REPLACEMENTS = [
    (re.compile(r"\.issues\b"), ".Issues"),
    (re.compile(r"\.severity\b"), ".Severity"),
    (re.compile(r"\.where\b"), ".Where"),
    (re.compile(r"\.message\b"), ".Message"),
    (re.compile(r"\.ok\b"), ".Ok"),
    (re.compile(r"\.error\b"), ".Error"),
    (re.compile(r"report\.ok\(\)"), "report.Ok()"),
    (re.compile(r"->ok\(\)"), "->Ok()"),
    (re.compile(r"\.ok\(\)"), ".Ok()"),
    (re.compile(r"\.error\("), ".Error("),
    (re.compile(r"\.warning\("), ".Warning("),
]


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    if any(s in path.parts for s in SKIP_DIRS):
        return False
    return True


def main() -> None:
    changed = []
    for path in sorted(ROOT.rglob("*")):
        if not path.is_file() or not should_process(path):
            continue
        original = path.read_text(encoding="utf-8")
        updated = original
        for old, new in LITERAL_REPLACEMENTS:
            updated = updated.replace(old, new)
        for pattern, repl in REGEX_REPLACEMENTS:
            updated = pattern.sub(repl, updated)
        if updated != original:
            path.write_text(updated, encoding="utf-8")
            changed.append(path.relative_to(ROOT))
    print(f"Patched {len(changed)} files")
    for p in changed:
        print(f"  {p}")


if __name__ == "__main__":
    main()
