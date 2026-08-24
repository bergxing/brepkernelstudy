#!/usr/bin/env python3
"""Align viewer .cpp method names with header declarations from style migration."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "apps" / "viewer"

# (old, new) replacements applied in order (longer / more specific first).
REPLACEMENTS = [
    ("PickRenderables_in_rect", "PickRenderablesInRect"),
    ("consume_CameraDirty", "ConsumeCameraDirty"),
    ("clear_selected_tags", "Clearselected_tags"),
    ("clear_snap_feedback", "Clearsnap_feedback"),
    ("AccuSnap::ClearFeedback", "AccuSnap::Clearfeedback"),
    ("create_demo_box_scene", "CreateDemoBoxScene"),
    ("create_blank_scene", "CreateBlankScene"),
    ("set_document_path", "Setdocument_path"),
    ("set_export_path", "Setexport_path"),
    ("pointer_double_click", "Pointerdouble_click"),
    ("forward_tool_press", "Forwardtool_press"),
    ("restore_idle_cursor", "Restoreidle_cursor"),
    ("update_rubber_band", "Updaterubber_band"),
    ("hide_rubber_band", "Hiderubber_band"),
    ("fit_view_to_scene", "Fitview_to_scene"),
    ("begin_right_press", "Beginright_press"),
    ("pointer_release", "Pointerrelease"),
    ("pointer_press", "Pointerpress"),
    ("pointer_wheel", "Pointerwheel"),
    ("pointer_move", "Pointermove"),
    ("sync_renderer", "Syncrenderer"),
    ("sync_from_world", "Syncfrom_world"),
    ("apply_key", "Applykey"),
    ("SetHighlightEdges", "Sethighlight_edges"),
    ("ClearHighlight", "Clearhighlight"),
    ("SetMaterial", "Setmaterial"),
    ("SetMeshes", "Setmeshes"),
    ("CreateSelectionAlbedoTexture", "Createselection_albedo_texture"),
    ("CreateAlbedoTexture", "Createalbedo_texture"),
    ("update_albedo_descriptors", "Updatealbedo_descriptors"),
    ("CreateDescriptors", "Createdescriptors"),
    ("CreatePipelines", "Createpipelines"),
    ("upload_selection_meshes", "Uploadselection_meshes"),
    ("upload_colored_edges", "Uploadcolored_edges"),
    ("upload_preview_solid", "Uploadpreview_solid"),
    ("upload_snap_overlay", "Uploadsnap_overlay"),
    ("destroy_texture", "Destroytexture"),
    ("destroy_buffer", "Destroybuffer"),
    ("upload_highlight", "Uploadhighlight"),
    ("upload_meshes", "Uploadmeshes"),
    ("upload_preview", "Uploadpreview"),
    ("upload_axes", "Uploadaxes"),
    ("UpdatePreview", "Updatepreview"),
    ("PickHeight", "Pickheight"),
    ("PickGround", "Pickground"),
    ("commit_copies", "Commitcopies"),
    ("commit_sphere", "Commitsphere"),
    ("CommitBox", "Commitbox"),
    ("pick_point", "Pickpoint"),
    ("rebuild_list", "RebuildList"),
    ("ctx.session->Path()", "ctx.session->path()"),
    ("prompt()", "Prompt()"),
    (".kind = SnapKind::Grid, .point =", ".Kind = SnapKind::Grid, .Point ="),
    ("best->point", "best->Point"),
]

EXTENSIONS = {".cpp", ".h"}


def patch_file(path: Path) -> bool:
    text = path.read_text(encoding="utf-8")
    original = text
    for old, new in REPLACEMENTS:
        text = text.replace(old, new)
    if text != original:
        path.write_text(text, encoding="utf-8")
        return True
    return False


def main() -> None:
    changed = []
    for path in sorted(ROOT.rglob("*")):
        if path.suffix.lower() not in EXTENSIONS:
            continue
        if patch_file(path):
            changed.append(path.relative_to(ROOT.parent.parent))
    print(f"Patched {len(changed)} files")
    for p in changed:
        print(f"  {p}")


if __name__ == "__main__":
    main()
