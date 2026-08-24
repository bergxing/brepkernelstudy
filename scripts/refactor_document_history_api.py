#!/usr/bin/env python3
"""Update viewer DocumentHistory call sites."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {"third_party", "cmake-build-mingw-debug", ".git", "build", "install-kernel"}

METHODS = [
    ("DocumentHistory::push", "DocumentHistory::Push"),
    ("DocumentHistory::clear", "DocumentHistory::Clear"),
    ("DocumentHistory::can_undo", "DocumentHistory::CanUndo"),
    ("DocumentHistory::can_redo", "DocumentHistory::CanRedo"),
    ("DocumentHistory::undo_label", "DocumentHistory::UndoLabel"),
    ("DocumentHistory::redo_label", "DocumentHistory::RedoLabel"),
    ("DocumentHistory::undo", "DocumentHistory::Undo"),
    ("DocumentHistory::redo", "DocumentHistory::Redo"),
]

CALL_PATTERNS = [
    ("can_undo", "CanUndo"),
    ("can_redo", "CanRedo"),
    ("undo_label", "UndoLabel"),
    ("redo_label", "RedoLabel"),
]


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    if "apps" not in path.parts or "viewer" not in path.parts:
        return False
    return not any(skip in path.parts for skip in SKIP_DIRS)


def replace_history(text: str) -> str:
    for old, new in METHODS:
        text = text.replace(old, new)
    for old, new in CALL_PATTERNS:
        text = re.sub(rf"history(\(\)|->)\.{re.escape(old)}\(", rf"history\1.{new}(", text)
        text = re.sub(rf"history(\(\)|->)\.{re.escape(old)}\b", rf"history\1.{new}", text)
    text = re.sub(r"ctx\.history->can_undo\(\)", "ctx.history->CanUndo()", text)
    text = re.sub(r"ctx\.history->can_redo\(\)", "ctx.history->CanRedo()", text)
    text = re.sub(r"ctx\.history->undo_label\(\)", "ctx.history->UndoLabel()", text)
    text = re.sub(r"ctx\.history->redo_label\(\)", "ctx.history->RedoLabel()", text)
    text = re.sub(r"history->push\(", "history->Push(", text)
    text = re.sub(r"history_\.clear\(", "history_.Clear(", text)
    text = re.sub(r"history->undo\(\)", "history->Undo()", text)
    text = re.sub(r"history->redo\(\)", "history->Redo()", text)
    return text


def process_file(path: Path) -> bool:
    original = path.read_text(encoding="utf-8")
    updated = replace_history(original)
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
