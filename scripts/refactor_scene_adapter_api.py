#!/usr/bin/env python3
"""Update SceneAdapter API call sites after PascalCase refactor."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SKIP_DIRS = {"third_party", "cmake-build-mingw-debug", ".git", "build", "install-kernel"}

METHODS = [
    ("create_blank", "CreateBlank"),
    ("main_part_const", "MainPartConst"),
    ("main_part", "MainPart"),
    ("mesh_for_body", "MeshForBody"),
    ("object_for_feature", "ObjectForFeature"),
    ("object_for_body", "ObjectForBody"),
    ("record_append_feature", "RecordAppendFeature"),
    ("record_append_sphere", "RecordAppendSphere"),
    ("set_box_params", "SetBoxParams"),
    ("set_sphere_params", "SetSphereParams"),
    ("last_regen_detail", "LastRegenDetail"),
    ("boolean_params", "GetBooleanParams"),
    ("sphere_params", "GetSphereParams"),
    ("box_params", "GetBoxParams"),
    ("feature_id_for", "FeatureIdFor"),
    ("remove_feature", "RemoveFeature"),
    ("undo_feature", "UndoFeature"),
    ("redo_feature", "RedoFeature"),
    ("box_spec_for", "BoxSpecFor"),
    ("add_boolean", "AddBoolean"),
    ("add_sphere", "AddSphere"),
    ("add_box", "AddBox"),
]

INCLUDE_FIXES = [
    ('"adapter/Sceneadapter.h"', '"adapter/SceneAdapter.h"'),
    ('"adapter/SceneAdapter.h"', '"adapter/SceneAdapter.h"'),
    ('"adapter/scene_adapter.h"', '"adapter/SceneAdapter.h"'),
]


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    return not any(skip in path.parts for skip in SKIP_DIRS)


def process_file(path: Path) -> bool:
    original = path.read_text(encoding="utf-8")
    updated = original
    for old, new in INCLUDE_FIXES:
        updated = updated.replace(old, new)
    for old, new in METHODS:
        updated = re.sub(rf"(\.|->){re.escape(old)}\(", rf"\1{new}(", updated)
        updated = updated.replace(f"SceneAdapter::{old}(", f"SceneAdapter::{new}(")
    # scene.document() accessor on SceneAdapter
    updated = re.sub(r"scene\.document\(\)", "scene.Document()", updated)
    updated = re.sub(r"loaded_scene\.document\(\)", "loaded_scene.Document()", updated)
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
