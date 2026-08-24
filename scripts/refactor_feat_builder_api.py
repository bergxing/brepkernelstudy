#!/usr/bin/env python3
"""Bulk-update FeatureTree, ParameterStore, IFeature, and Builder call sites."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {"third_party", "cmake-build-mingw-debug", ".git", "build", "install-kernel"}

# FeatureTree methods after .Features(). or tree.
TREE_METHODS = [
    ("find_by_body", "FindByBody"),
    ("mark_dirty_from", "MarkDirtyFrom"),
    ("mark_all_dirty", "MarkAllDirty"),
    ("first_dirty_index", "FirstDirtyIndex"),
    ("rollback_index", "RollbackIndex"),
    ("set_rollback", "SetRollback"),
    ("append", "Append"),
    ("remove", "Remove"),
    ("find", "Find"),
]

# Old vector accessor on tree (not Part)
TREE_FEATURES_VEC = [
    (".Features().features()", ".Features().Features()"),
    ("->Features().features()", "->Features().Features()"),
    (".features().features()", ".Features().Features()"),
]

PARAM_METHODS = [
    ("add_with_id", "AddWithId"),
    ("set_by_name", "SetByName"),
    ("get_by_name", "GetByName"),
    ("find_by_name", "FindByName"),
    ("clear_dirty", "ClearDirty"),
    ("mark_dirty", "MarkDirty"),
    ("append", "Append"),  # skip - not param
]

PARAM_CHAIN = [
    ("add_with_id", "AddWithId"),
    ("set_by_name", "SetByName"),
    ("get_by_name", "GetByName"),
    ("find_by_name", "FindByName"),
    ("clear_dirty", "ClearDirty"),
    ("mark_dirty", "MarkDirty"),
    ("add", "Add"),
    ("set", "Set"),
    ("get", "Get"),
    ("all", "All"),
    ("find", "Find"),
    ("remove", "Remove"),
]

IFEATURE_METHODS = [
    ("set_body_guid", "SetBodyGuid"),
    ("set_suppressed", "SetSuppressed"),
    ("set_status", "SetStatus"),
    ("collect_parameters", "CollectParameters"),
    ("display_name", "DisplayName"),
    ("type_name", "TypeName"),
    ("body_guid", "BodyGuid"),
    ("suppressed", "Suppressed"),
    ("rebuild", "Rebuild"),
    ("status", "Status"),
]

FEATURE_SPECIFIC = [
    ("SketchFeature::create_rectangle", "SketchFeature::CreateRectangle"),
    ("ExtrudeFeature::create", "ExtrudeFeature::Create"),
    ("BooleanFeature::create", "BooleanFeature::Create"),
    ("SphereFeature::create", "SphereFeature::Create"),
    ("BoxFeature::create", "BoxFeature::Create"),
    ("target_feature_id", "TargetFeatureId"),
    ("tool_feature_id", "ToolFeatureId"),
    ("sketch_feature_id", "SketchFeatureId"),
    ("distance_id", "DistanceId"),
    ("length_id", "LengthId"),
    ("width_id", "WidthId"),
    ("height_id", "HeightId"),
    ("radius_id", "RadiusId"),
    ("to_spec", "ToSpec"),
    ("->op()", "->Op()"),
    (".op()", ".Op()"),
]

BUILDER = [
    ("make_box", "MakeBox"),
    ("make_sphere", "MakeSphere"),
]

CMD_LINE_MARKERS = ("cmd", "command", "Command", "active_tool", "ITool", "ICommand")


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    return not any(skip in path.parts for skip in SKIP_DIRS)


def replace_tree_methods(text: str) -> str:
    for old, new in TREE_METHODS:
        text = re.sub(rf"\.Features\(\)\.{re.escape(old)}\(", f".Features().{new}(", text)
        text = re.sub(rf"\.features\(\)\.{re.escape(old)}\(", f".Features().{new}(", text)
        text = re.sub(rf"\btree\.{re.escape(old)}\(", f"tree.{new}(", text)
        text = re.sub(rf"\bm_features\.{re.escape(old)}\(", f"m_features.{new}(", text)
    for old, new in TREE_FEATURES_VEC:
        text = text.replace(old, new)
    return text


def replace_param_methods(text: str) -> str:
    for old, new in PARAM_CHAIN:
        text = re.sub(rf"\.Parameters\(\)\.{re.escape(old)}\(", f".Parameters().{new}(", text)
        text = re.sub(rf"\.parameters\(\)\.{re.escape(old)}\(", f".Parameters().{new}(", text)
        text = re.sub(rf"\bparams\.{re.escape(old)}\(", f"params.{new}(", text)
        text = re.sub(rf"\bstore\.{re.escape(old)}\(", f"store.{new}(", text)
        text = re.sub(rf"->Parameters\(\)\.{re.escape(old)}\(", f"->Parameters().{new}(", text)
    return text


def replace_ifeature_methods(text: str) -> str:
    for old, new in IFEATURE_METHODS:
        text = re.sub(rf"(->|\.){re.escape(old)}\(", rf"\1{new}(", text)
    return text


def replace_feature_id(text: str) -> str:
    def repl(match: re.Match[str]) -> str:
        start = text.rfind("\n", 0, match.start()) + 1
        line = text[start : match.end()]
        if any(m in line for m in CMD_LINE_MARKERS):
            return match.group(0)
        return match.group(0).replace("->id()", "->Id()")

    text = re.sub(r"->id\(\)", repl, text)
    text = re.sub(r"->find\([^)]+\)->id\(\)", lambda m: m.group(0).replace("->id()", "->Id()"), text)
    text = re.sub(r"->find_by_body\([^)]+\)->id\(\)", lambda m: m.group(0).replace("->find_by_body", "->FindByBody").replace("->id()", "->Id()"), text)
    return text


def process_file(path: Path) -> bool:
    original = path.read_text(encoding="utf-8")
    updated = original
    for old, new in BUILDER:
        updated = re.sub(rf"\b{re.escape(old)}\(", f"{new}(", updated)
    for old, new in FEATURE_SPECIFIC:
        updated = updated.replace(old, new)
    updated = replace_tree_methods(updated)
    updated = replace_param_methods(updated)
    updated = replace_ifeature_methods(updated)
    updated = replace_feature_id(updated)
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
