#!/usr/bin/env python3
"""Update Part/Document API call sites after PascalCase refactor."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SKIP_DIRS = {
    "third_party",
    "cmake-build-mingw-debug",
    "cmake-build-mingw-release",
    ".git",
    "build",
}

PART_METHODS = [
    ("clear_last_regen_detail", "ClearLastRegenDetail"),
    ("set_last_regen_detail", "SetLastRegenDetail"),
    ("last_regen_detail", "LastRegenDetail"),
    ("set_boolean_evaluator", "SetBooleanEvaluator"),
    ("boolean_evaluator", "BooleanEvaluator"),
    ("rebuild_boolean_body", "RebuildBooleanBody"),
    ("rebuild_extrude_body", "RebuildExtrudeBody"),
    ("rebuild_sphere_body", "RebuildSphereBody"),
    ("rebuild_box_body", "RebuildBoxBody"),
    ("edit_feature_params", "EditFeatureParams"),
    ("add_rectangle_sketch", "AddRectangleSketch"),
    ("unregister_body", "UnregisterBody"),
    ("register_body", "RegisterBody"),
    ("remove_feature", "RemoveFeature"),
    ("feature_history", "FeatureHistory"),
    ("add_boolean", "AddBoolean"),
    ("add_extrude", "AddExtrude"),
    ("add_sphere", "AddSphere"),
    ("add_box", "AddBox"),
    ("find_body", "FindBody"),
    ("regenerate", "Regenerate"),
    ("parameters", "Parameters"),
    ("features", "Features"),
    ("model", "Model"),
]

DOC_METHODS = [
    ("mark_clean", "MarkClean"),
    ("mark_dirty", "MarkDirty"),
    ("add_part", "AddPart"),
    ("main_part", "MainPart"),
    ("set_path", "SetPath"),
    ("assembly", "Assembly"),
    ("parts", "Parts"),
]

DOC_REGISTRY = re.compile(
    r"(doc(?:ument)?(?:_->)?|m_document->|loaded\.document->|scene\.document\(\)->|"
    r"document\(\)->|Document\* [a-zA-Z_]+\s*=\s*[^;]+;\s*[^;]*?)registry\("
)

IOBJECT_KIND = re.compile(r"\.kind\(\)")


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    parts = path.parts
    return not any(skip in parts for skip in SKIP_DIRS)


def replace_part_methods(text: str) -> str:
    for old, new in PART_METHODS:
        text = re.sub(rf"(->|\.){re.escape(old)}\(", rf"\1{new}(", text)
    return text


def replace_doc_methods(text: str) -> str:
    text = text.replace("Document::create", "Document::Create")
    for old, new in DOC_METHODS:
        text = re.sub(rf"(->|\.){re.escape(old)}\(", rf"\1{new}(", text)
    # Document registry — avoid entt world registry().
    text = re.sub(r"(->|\.)registry\(", _replace_registry, text)
    text = re.sub(r"(->|\.)path\(\)", r"\1Path()", text)
    text = re.sub(r"(->|\.)dirty\(\)", r"\1Dirty()", text)
    return text


def _replace_registry(match: re.Match[str]) -> str:
    prefix = match.group(1)
    start = match.string.rfind("\n", 0, match.start()) + 1
    line = match.string[start : match.end()]
    if "world" in line.lower():
        return match.group(0)
    return f"{prefix}Registry("


def replace_document_clear(text: str) -> str:
    return re.sub(
        r"\b(doc|document|m_document|loaded\.document|scene\.document\(\))(->|\.)Clear\(",
        r"\1\2Clear(",
        text,
    )


def replace_set_document(text: str) -> str:
    return text.replace("set_document(", "SetDocument(")


def replace_iobject_kind(text: str) -> str:
    return IOBJECT_KIND.sub(".Kind()", text)


def process_file(path: Path) -> bool:
    original = path.read_text(encoding="utf-8")
    updated = original
    updated = replace_part_methods(updated)
    updated = replace_doc_methods(updated)
    updated = replace_document_clear(updated)
    updated = replace_set_document(updated)
    updated = replace_iobject_kind(updated)
    if updated != original:
        path.write_text(updated, encoding="utf-8")
        return True
    return False


def main() -> None:
    changed = 0
    for path in ROOT.rglob("*"):
        if not path.is_file() or not should_process(path):
            continue
        if process_file(path):
            changed += 1
            print(path.relative_to(ROOT))
    print(f"Updated {changed} files.")


if __name__ == "__main__":
    main()
