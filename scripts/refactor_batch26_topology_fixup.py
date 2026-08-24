#!/usr/bin/env python3
"""Batch 26 topology fixup: remaining field access patterns."""

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

REGEX = [
    (re.compile(r"->t0\b"), "->T0"),
    (re.compile(r"->t1\b"), "->T1"),
    (re.compile(r"->tolerance\b"), "->Tolerance"),
    (re.compile(r"->radial\b"), "->Radial"),
    (re.compile(r"->sense\b"), "->Sense"),
    (re.compile(r"\.sense\b"), ".Sense"),
    (re.compile(r"\.surface\b"), ".Surface"),
    (re.compile(r"\.tolerance\b"), ".Tolerance"),
    (re.compile(r"\.radial\b"), ".Radial"),
    (re.compile(r"!point\b"), "!Point"),
    (re.compile(r"\bpoint->"), "Point->"),
    (re.compile(r"\bsurface->"), "Surface->"),
    (re.compile(r"\bfor \(Loop\* l : loops\b"), "for (Loop* l : Loops"),
    (re.compile(r"return loops\.empty"), "return Loops.empty"),
    (re.compile(r"return loops\.front"), "return Loops.front"),
    (re.compile(r"return sense == Orientation"), "return Sense == Orientation"),
    (re.compile(r"!first\b"), "!First"),
    (re.compile(r"\bfirst->"), "First->"),
    (re.compile(r"CoEdge\* c = first\b"), "CoEdge* c = First"),
    (re.compile(r"c != first\b"), "c != First"),
    (re.compile(r"c->sense = sense"), "c->Sense = sense"),
    (re.compile(r"f->sense = sense"), "f->Sense = sense"),
    (re.compile(r"e->radial\.push_back"), "e->Radial.push_back"),
    (re.compile(r"c\.Edge->radial"), "c.Edge->Radial"),
    (re.compile(r"ce\.Edge->radial"), "ce.Edge->Radial"),
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
        for pattern, repl in REGEX:
            updated = pattern.sub(repl, updated)
        if updated != text:
            path.write_text(updated, encoding="utf-8")
            changed.append(path.relative_to(ROOT))
    print(f"Patched {len(changed)} files")
    for p in changed:
        print(f"  {p}")


if __name__ == "__main__":
    main()
