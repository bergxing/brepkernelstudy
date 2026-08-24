#!/usr/bin/env python3
"""Batch 15: Viewer + kernel free functions and selected methods."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {"third_party", "cmake-build-mingw-debug", "build-kernel", "build-mingw", ".git", "build", "install-kernel"}

REPLACEMENTS = [
    # Kernel free functions (batch 13)
    ("tessellate_face", "TessellateFace"),
    ("tessellate_body", "TessellateBody"),
    ("extract_edges", "ExtractEdges"),
    ("validate_body", "ValidateBody"),
    ("dump_body", "DumpBody"),
    ("init_logging", "InitLogging"),
    ("log_message", "LogMessage"),
    ("query_snap_candidates", "QuerySnapCandidates"),
    ("save_xl", "SaveXl"),
    ("load_xl", "LoadXl"),
    ("last_xl_error", "LastXlError"),
    ("bks_cache_path_for", "BksCachePathFor"),
    ("save_bks_cache", "SaveBksCache"),
    ("load_bks_cache", "LoadBksCache"),
    ("sample_loop", "SampleLoop"),
    ("group_face_regions", "GroupFaceRegions"),
    ("unwrap_sphere_ring", "UnwrapSphereRing"),
    ("sample_edge_xyz", "SampleEdgeXyz"),
    ("triangulate_constrained", "TriangulateConstrained"),
    ("triangulate_polygon_with_holes", "TriangulatePolygonWithHoles"),
    ("estimate_face_aabb", "EstimateFaceAabb"),
    ("evaluate_box_boolean", "EvaluateBoxBoolean"),
    ("evaluate_sphere_sphere_boolean", "EvaluateSphereSphereBoolean"),
    ("evaluate_prism_box_boolean", "EvaluatePrismBoxBoolean"),
    ("evaluate_sphere_box_boolean", "EvaluateSphereBoxBoolean"),
    ("recognize_axis_aligned_box", "RecognizeAxisAlignedBox"),
    ("recognize_analytic_sphere", "RecognizeAnalyticSphere"),
    ("recognize_extrusion_prism", "RecognizeExtrusionPrism"),
    ("intersect_plane_sphere", "IntersectPlaneSphere"),
    ("intersect_sphere_sphere", "IntersectSphereSphere"),
    ("intersect_plane_plane", "IntersectPlanePlane"),
    ("intersect_plane_cylinder", "IntersectPlaneCylinder"),
    ("intersect_sphere_cylinder", "IntersectSphereCylinder"),
    ("classify_point_in_box", "ClassifyPointInBox"),
    ("classify_point_in_sphere", "ClassifyPointInSphere"),
    ("collect_face_pair_candidates", "CollectFacePairCandidates"),
    ("probe_face_pair_intersections", "ProbeFacePairIntersections"),
    ("sense_as_int(", "SenseAsInt("),
    ("brep::opposite(", "brep::Opposite("),
    # Viewer ecs systems
    ("input_on_press", "InputOnPress"),
    ("input_on_move", "InputOnMove"),
    ("input_on_release", "InputOnRelease"),
    ("pending_click_is_multi", "PendingClickIsMulti"),
    ("input_on_wheel", "InputOnWheel"),
    ("input_on_key", "InputOnKey"),
    ("render_sync", "RenderSync"),
    ("consume_camera_dirty", "ConsumeCameraDirty"),
    ("pick_renderable", "PickRenderable"),
    ("pick_renderables_in_rect", "PickRenderablesInRect"),
    ("scene_aabb", "SceneAabb"),
    ("fit_camera_to_scene", "FitCameraToScene"),
    ("set_selection", "SetSelection"),
    ("toggle_selection", "ToggleSelection"),
    ("select_entities", "SelectEntities"),
    ("clear_selection", "ClearSelection"),
    ("selected_entity", "SelectedEntity"),
    ("selected_count", "SelectedCount"),
    ("selection_order", "SelectionOrder"),
    ("selection_label", "SelectionLabel"),
    # Picking
    ("screen_to_ray", "ScreenToRay"),
    ("world_to_screen", "WorldToScreen"),
    ("intersect_plane_y", "IntersectPlaneY"),
    ("intersect_plane(", "IntersectPlane("),
    ("intersect_mesh", "IntersectMesh"),
    # Snap settings
    ("default_snap_kinds", "DefaultSnapKinds"),
    ("load_snap_settings", "LoadSnapSettings"),
    ("save_snap_settings", "SaveSnapSettings"),
    ("append_edge_segments", "AppendEdgeSegments"),
    ("run_viewer", "RunViewer"),
    # World
    ("clear_scene", "ClearScene"),
    ("create_body_renderable", "CreateBodyRenderable"),
    ("destroy_body_renderable", "DestroyBodyRenderable"),
    ("find_body_renderable", "FindBodyRenderable"),
    ("update_body_renderable", "UpdateBodyRenderable"),
    ("sync_part_bodies", "SyncPartBodies"),
    # Camera methods (common)
    ("set_yaw_pitch", "SetYawPitch"),
    ("fit_sphere", "FitSphere"),
    ("set_standard_view", "SetStandardView"),
    ("keep_outside_demo_box", "KeepOutsideDemoBox"),
    ("exit_framed_to_orbit", "ExitFramedToOrbit"),
    ("view_matrix", "ViewMatrix"),
    ("proj_matrix", "ProjMatrix"),
    ("eye()", "Eye()"),
    # Document
    ("mark_dirty", "MarkDirty"),
    ("new_blank_document", "NewBlankDocument"),
    ("sync_title_from_world", "SyncTitleFromWorld"),
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
