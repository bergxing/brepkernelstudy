#!/usr/bin/env python3
"""Update FeatureHistory / FeatureTransaction call sites."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {"third_party", "cmake-build-mingw-debug", ".git", "build", "install-kernel"}

TX_FIELDS = [
    ("target_was_suppressed", "TargetWasSuppressed"),
    ("tool_was_suppressed", "ToolWasSuppressed"),
    ("extrude_distance", "ExtrudeDistance"),
    ("sketch_feature", "SketchFeatureId"),
    ("feature_type", "FeatureType"),
    ("boolean_op", "Op"),
    ("box_origin", "BoxOrigin"),
    ("sketch_name", "SketchName"),
    ("sphere_spec", "Sphere"),
    ("box_spec", "Box"),
    ("param_before", "ParamBefore"),
    ("param_after", "ParamAfter"),
    ("was_suppressed", "WasSuppressed"),
    ("feature", "Feature"),
    ("kind", "Kind"),
]

HISTORY_METHODS = [
    ("apply_and_record", "ApplyAndRecord"),
    ("can_undo", "CanUndo"),
    ("can_redo", "CanRedo"),
    ("record", "Record"),
    ("undo", "Undo"),
    ("redo", "Redo"),
    ("clear", "Clear"),
]

REGENERATOR = [
    ("Regenerator::run", "Regenerator::Run"),
]


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    return not any(skip in path.parts for skip in SKIP_DIRS)


def replace_tx_fields(text: str) -> str:
    for old, new in TX_FIELDS:
        text = re.sub(rf"\btx\.{re.escape(old)}\b", f"tx.{new}", text)
    return text


def replace_history_methods(text: str) -> str:
    for old, new in HISTORY_METHODS:
        # FeatureHistory().method and m_history.method and ->FeatureHistory().method
        text = re.sub(rf"(\.|->)FeatureHistory\(\)\.{re.escape(old)}\(", rf"\1FeatureHistory().{new}(", text)
        text = re.sub(rf"\bm_history\.{re.escape(old)}\(", f"m_history.{new}(", text)
        text = re.sub(rf"(\.|->)feature_history\(\)\.{re.escape(old)}\(", rf"\1FeatureHistory().{new}(", text)
    return text


def process_file(path: Path) -> bool:
    original = path.read_text(encoding="utf-8")
    updated = original
    for old, new in REGENERATOR:
        updated = updated.replace(old, new)
    updated = replace_tx_fields(updated)
    updated = replace_history_methods(updated)
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
