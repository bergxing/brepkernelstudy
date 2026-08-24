#!/usr/bin/env python3
"""Batch 12: Safe targeted struct field renames (headers updated first)."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {"third_party", "cmake-build-mingw-debug", "build-kernel", "build-mingw", ".git", "build", "install-kernel"}

REPLACEMENTS = [
    ("make_wood_material", "MakeWoodMaterial"),
    ("material_.albedo_path", "material_.AlbedoPath"),
    ("material_.uv_scale", "material_.UvScale"),
    ("material_.albedo_color", "material_.AlbedoColor"),
    (".albedo_path", ".AlbedoPath"),
    (".uv_scale", ".UvScale"),
    (".albedo_color", ".AlbedoColor"),
    ("Plane::xz_y_up()", "Plane::XzYUp()"),
    ("Plane::xy()", "Plane::Xy()"),
    (".to_world(", ".ToWorld("),
    ("plane.origin", "plane.Origin"),
    ("plane.normal", "plane.Normal"),
    ("plane.u_axis", "plane.UAxis"),
    ("plane.v_axis", "plane.VAxis"),
    ("->Transform.translation", "->Transform.Translation"),
    ("->Transform.x_axis", "->Transform.XAxis"),
    ("->Transform.y_axis", "->Transform.YAxis"),
    ("->Transform.z_axis", "->Transform.ZAxis"),
    (".Transform.translation", ".Transform.Translation"),
    (".Transform.x_axis", ".Transform.XAxis"),
    (".Transform.y_axis", ".Transform.YAxis"),
    (".Transform.z_axis", ".Transform.ZAxis"),
    ("xf.translation", "xf.Translation"),
    ("candidate.kind", "candidate.Kind"),
    ("candidate.body_guid", "candidate.BodyGuid"),
    ("best->kind", "best->Kind"),
    ("query.kinds", "query.Kinds"),
    ("query.near_point", "query.NearPoint"),
    ("query.reference_point", "query.ReferencePoint"),
    ("query.tolerance", "query.Tolerance"),
    ("opts.linear_deflection", "opts.LinearDeflection"),
    ("opts.angular_deflection", "opts.AngularDeflection"),
    ("opts.min_u_segments", "opts.MinUSegments"),
    ("opts.min_v_segments", "opts.MinVSegments"),
    ("opts.max_u_segments", "opts.MaxUSegments"),
    ("opts.max_v_segments", "opts.MaxVSegments"),
    ("opts.include_seam_edges", "opts.IncludeSeamEdges"),
    ("TessellationOptions::for_radius", "TessellationOptions::ForRadius"),
    ("ctx.tol_3d", "ctx.Tol3d"),
    ("ctx.tol_2d", "ctx.Tol2d"),
    ("probe.candidate_pairs", "probe.CandidatePairs"),
    (".local_name", ".LocalName"),
    ("cache.document_guid", "cache.DocumentGuid"),
    ("expected_document_guid", "expectedDocumentGuid"),
    ("face_ref_a", "FaceRefA"),
    ("face_ref_b", "FaceRefB"),
    ("tol_3d", "Tol3d"),
    ("tol_2d", "Tol2d"),
    ("document_guid", "DocumentGuid"),
    ("linear_deflection", "LinearDeflection"),
    ("angular_deflection", "AngularDeflection"),
    ("min_u_segments", "MinUSegments"),
    ("min_v_segments", "MinVSegments"),
    ("max_u_segments", "MaxUSegments"),
    ("max_v_segments", "MaxVSegments"),
    ("include_seam_edges", "IncludeSeamEdges"),
    ("body_guid", "BodyGuid"),
    ("near_point", "NearPoint"),
    ("reference_point", "ReferencePoint"),
    ("wood_albedo_path", "WoodAlbedoPath"),
    ("boolean_info", "BooleanInfo"),
    ("scene_tri", "SceneTri"),
    ("scene_edges", "SceneEdges"),
    ("scene_material", "SceneMaterial"),
    ("have_scene", "HaveScene"),
    ("selected_tri", "SelectedTri"),
    ("selected_edges", "SelectedEdges"),
    ("selected_outline", "SelectedOutline"),
    ("have_selection", "HaveSelection"),
    ("renderable_count", "RenderableCount"),
    ("selection_count", "SelectionCount"),
    ("force_rebuild", "ForceRebuild"),
    ("drag_mode", "ActiveMode"),
    ("last_x", "LastX"),
    ("last_y", "LastY"),
    ("press_x", "PressX"),
    ("press_y", "PressY"),
    ("multi_select", "MultiSelect"),
    ("camera_dirty", "CameraDirty"),
    ("viewport_w", "ViewportW"),
    ("viewport_h", "ViewportH"),
    ("parent_widget", "ParentWidget"),
    ("view_camera", "ViewCamera"),
    ("snap_settings", "SnapSettingsPtr"),
    ("snap_session", "SnapSessionPtr"),
    ("report_status", "ReportStatus"),
    ("request_redraw", "RequestRedraw"),
    ("after_document_reset", "AfterDocumentReset"),
    ("refresh_ui", "RefreshUi"),
    ("set_preview_edges", "SetPreviewEdges"),
    ("set_preview", "SetPreview"),
    ("clear_preview", "ClearPreview"),
    ("set_snap_overlay", "SetSnapOverlay"),
    ("clear_snap_overlay", "ClearSnapOverlay"),
    ("refresh_cursor_tip", "RefreshCursorTip"),
    ("yaw_deg", "YawDeg"),
    ("pitch_deg", "PitchDeg"),
    ("ortho_half_h", "OrthoHalfH"),
    ("fov_deg", "FovDeg"),
    ("framed_forward", "FramedForward"),
    ("framed_up", "FramedUp"),
    ("grid_enabled", "GridEnabled"),
    ("grid_spacing", "GridSpacing"),
    ("aperture_px", "AperturePx"),
    ("last_point", "LastPoint"),
    ("axis_x", "AxisX"),
    ("axis_y", "AxisY"),
    ("dynamic_input_mode", "DynamicInputMode"),
    ("hold_override", "HoldOverride"),
    ("active_snap", "ActiveSnap"),
]


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    if any(s in path.parts for s in SKIP_DIRS):
        return False
    parts = path.parts
    for i in range(len(parts) - 2):
        if parts[i : i + 3] == ("apps", "viewer", "i18n"):
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
