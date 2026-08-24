#!/usr/bin/env python3
"""Batch 26b: Topology graph fields -> PascalCase struct members."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {"third_party", "cmake-build-mingw-debug", "build-kernel", "build-mingw", ".git", "build", "install-kernel"}

HEADER_LITERALS = [
    ("Point* point{", "Point* Point{"),
    ("std::vector<Edge*> edges;", "std::vector<Edge*> Edges;"),
    ("Curve* curve{", "Curve* Curve{"),
    ("Vertex* v0{", "Vertex* V0{"),
    ("Vertex* v1{", "Vertex* V1{"),
    ("double t0{", "double T0{"),
    ("double t1{", "double T1{"),
    ("std::vector<CoEdge*> radial;", "std::vector<CoEdge*> Radial;"),
    ("Edge* edge{", "Edge* Edge{"),
    ("Orientation sense{", "Orientation Sense{"),
    ("CoEdge* next{", "CoEdge* Next{"),
    ("CoEdge* prev{", "CoEdge* Prev{"),
    ("CoEdge* partner{", "CoEdge* Partner{"),
    ("Curve2d* pcurve{", "Curve2d* Pcurve{"),
    ("Loop* loop{", "Loop* Loop{"),
    ("Face* face{", "Face* Face{"),
    ("CoEdge* first{", "CoEdge* First{"),
    ("Surface* surface{", "Surface* Surface{"),
    ("std::vector<Loop*> loops;", "std::vector<Loop*> Loops;"),
    ("std::vector<Face*> faces;", "std::vector<Face*> Faces;"),
    ("bool closed{", "bool Closed{"),
    ("BodyType type{", "BodyType Type{"),
    ("std::vector<Shell*> shells;", "std::vector<Shell*> Shells;"),
    ("return sense == Orientation::Forward ? v0 : v1;",
     "return sense == Orientation::Forward ? V0 : V1;"),
    ("return sense == Orientation::Forward ? v1 : v0;",
     "return sense == Orientation::Forward ? V1 : V0;"),
    ("return sense == Orientation::Forward ? (t0 + (t1 - t0)",
     "return sense == Orientation::Forward ? (T0 + (T1 - T0)"),
    (": (t1 + (t0 - t1) * local_t);",
     ": (T1 + (T0 - T1) * local_t);"),
    ("return edge ? edge->Start(sense) : nullptr;",
     "return Edge ? Edge->Start(sense) : nullptr;"),
    ("return edge ? edge->End(sense) : nullptr;",
     "return Edge ? Edge->End(sense) : nullptr;"),
    ("c = c->next;", "c = c->Next;"),
    ("return loop ? loop->face : nullptr;",
     "return Loop ? Loop->Face : nullptr;"),
]

REGEX_REPLACEMENTS = [
    (re.compile(r"->point\b"), "->Point"),
    (re.compile(r"->v0\b"), "->V0"),
    (re.compile(r"->v1\b"), "->V1"),
    (re.compile(r"\.v0\b"), ".V0"),
    (re.compile(r"\.v1\b"), ".V1"),
    (re.compile(r"->curve\b"), "->Curve"),
    (re.compile(r"->edge\b"), "->Edge"),
    (re.compile(r"\.edge\b"), ".Edge"),
    (re.compile(r"->loop\b"), "->Loop"),
    (re.compile(r"->face\b"), "->Face"),
    (re.compile(r"->surface\b"), "->Surface"),
    (re.compile(r"->pcurve\b"), "->Pcurve"),
    (re.compile(r"->partner\b"), "->Partner"),
    (re.compile(r"->next\b"), "->Next"),
    (re.compile(r"->prev\b"), "->Prev"),
    (re.compile(r"->first\b"), "->First"),
    (re.compile(r"->shells\b"), "->Shells"),
    (re.compile(r"->faces\b"), "->Faces"),
    (re.compile(r"->loops\b"), "->Loops"),
    (re.compile(r"\.shells\b"), ".Shells"),
    (re.compile(r"\.faces\b"), ".Faces"),
    (re.compile(r"\.loops\b"), ".Loops"),
    (re.compile(r"\.t0\b"), ".T0"),
    (re.compile(r"\.t1\b"), ".T1"),
    (re.compile(r"\.radial\b"), ".Radial"),
    (re.compile(r"\.closed\b"), ".Closed"),
    (re.compile(r"->closed\b"), "->Closed"),
    (re.compile(r"\.edges\b"), ".Edges"),
    (re.compile(r"->edges\b"), "->Edges"),
    (re.compile(r"\.sense\b"), ".Sense"),
    (re.compile(r"body\.type\b"), "body.Type"),
    (re.compile(r"->type\b"), "->Type"),
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
        text = path.read_text(encoding="utf-8")
        updated = text
        for old, new in HEADER_LITERALS:
            updated = updated.replace(old, new)
        for pattern, repl in REGEX_REPLACEMENTS:
            updated = pattern.sub(repl, updated)
        if updated != text:
            path.write_text(updated, encoding="utf-8")
            changed.append(path.relative_to(ROOT))
    print(f"Patched {len(changed)} files")
    for p in changed:
        print(f"  {p}")


if __name__ == "__main__":
    main()
