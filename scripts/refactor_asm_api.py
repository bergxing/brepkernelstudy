#!/usr/bin/env python3
"""Batch 10: Assembly / Mate / Occurrence struct fields + Assembly methods."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {"third_party", "cmake-build-mingw-debug", "build-kernel", "build-mingw", ".git", "build", "install-kernel"}

REPLACEMENTS = [
    ("add_occurrence", "AddOccurrence"),
    ("add_mate", "AddMate"),
    ("occurrences()", "Occurrences()"),
    ("mates()", "Mates()"),
    ("Assembly::find(", "Assembly::Find("),
    ("assembly.find(", "assembly.Find("),
    ("MateSolver::solve", "MateSolver::Solve"),
    ("occ.part_guid", "occ.PartGuid"),
    ("o.part_guid", "o.PartGuid"),
    ("->part_guid", "->PartGuid"),
    (".part_guid", ".PartGuid"),
    ("occ.transform", "occ.Transform"),
    ("o.transform", "o.Transform"),
    ("a->transform", "a->Transform"),
    ("b->transform", "b->Transform"),
    ("mate.face_ref_a", "mate.FaceRefA"),
    ("mate.face_ref_b", "mate.FaceRefB"),
    ("mate2.face_ref_a", "mate2.FaceRefA"),
    ("mate2.face_ref_b", "mate2.FaceRefB"),
    ("mate.dim", "mate.Dim"),
    ("mate.aux", "mate.Aux"),
    ("mate.kind", "mate.Kind"),
    ("mate.id", "mate.Id"),
    ("Guid part_guid", "Guid partGuid"),
    ("part_guid,", "partGuid,"),
    ("part_guid)", "partGuid)"),
    ("params->get(", "params->Get("),
]


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    if any(s in path.parts for s in SKIP_DIRS):
        return False
    if "apps" in path.parts and "viewer" not in path.parts:
        return False
    return True


def process_file(path: Path) -> bool:
    original = path.read_text(encoding="utf-8")
    updated = original
    for old, new in REPLACEMENTS:
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
