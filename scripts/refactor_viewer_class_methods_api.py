#!/usr/bin/env python3
"""Batch 15b: Viewer class method names snake_case -> PascalCase."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {"third_party", "cmake-build-mingw-debug", ".git", "build"}

REPLACEMENTS = [
    ("void set_", "void Set"),
    ("void on_", "void On"),
    ("void handle_", "void Handle"),
    ("void pointer_", "void Pointer"),
    ("void sync_", "void Sync"),
    ("void update_", "void Update"),
    ("void create_", "void Create"),
    ("void upload_", "void Upload"),
    ("void destroy_", "void Destroy"),
    ("void begin_", "void Begin"),
    ("void hide_", "void Hide"),
    ("void restore_", "void Restore"),
    ("void fit_", "void Fit"),
    ("void apply_", "void Apply"),
    ("void notify_", "void Notify"),
    ("void project_", "void Project"),
    ("void wire_", "void Wire"),
    ("void setup_", "void Setup"),
    ("void refresh_", "void Refresh"),
    ("void clear_", "void Clear"),
    ("bool pick_", "bool Pick"),
    ("bool forward_", "bool Forward"),
    ("bool maybe_", "bool Maybe"),
    ("bool commit_", "bool Commit"),
    ("void commit_", "void Commit"),
    ("->set_", "->Set"),
    ("->on_", "->On"),
    ("->pointer_", "->Pointer"),
    ("->sync_", "->Sync"),
    ("->handle_", "->Handle"),
    ("->wire_", "->Wire"),
    ("->setup_", "->Setup"),
    ("->refresh_", "->Refresh"),
    ("->clear_", "->Clear"),
    ("->pick_", "->Pick"),
    ("->commit_", "->Commit"),
    ("->update_", "->Update"),
    ("->create_", "->Create"),
    ("->upload_", "->Upload"),
    ("->fit_", "->Fit"),
    ("->apply_", "->Apply"),
    ("create_camera", "CreateCamera"),
    ("create_body_renderable", "CreateBodyRenderable"),
    ("set_adapter", "SetAdapter"),
    ("set_meshes", "SetMeshes"),
    ("set_material", "SetMaterial"),
    ("set_highlight_edges", "SetHighlightEdges"),
    ("set_preview", "SetPreview"),
    ("set_world", "SetWorld"),
    ("set_rubber_band_host", "SetRubberBandHost"),
    ("set_tool_motion_callback", "SetToolMotionCallback"),
    ("set_tool_press_callback", "SetToolPressCallback"),
    ("set_context_menu_callback", "SetContextMenuCallback"),
    ("set_redraw_callback", "SetRedrawCallback"),
    ("set_camera", "SetCamera"),
    ("set_selection_callback", "SetSelectionCallback"),
    ("clear_highlight", "ClearHighlight"),
    ("clear_feedback", "ClearFeedback"),
    ("clear_snap_overlay", "ClearSnapOverlay"),
    ("pick_ground", "PickGround"),
    ("pick_height", "PickHeight"),
    ("update_preview", "UpdatePreview"),
    ("commit_box", "CommitBox"),
    ("on_dim_edited", "OnDimEdited"),
    ("refresh_dim_hint", "RefreshDimHint"),
    ("ensure_initialized", "EnsureInitialized"),
    ("transition_image_layout", "TransitionImageLayout"),
    ("create_pipelines", "CreatePipelines"),
    ("create_descriptors", "CreateDescriptors"),
    ("create_albedo_texture", "CreateAlbedoTexture"),
    ("create_selection_albedo_texture", "CreateSelectionAlbedoTexture"),
]


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    if any(s in path.parts for s in SKIP_DIRS):
        return False
    if "apps" not in path.parts or "viewer" not in path.parts:
        return False
    if "i18n" in path.parts:
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
