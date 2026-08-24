#!/usr/bin/env python3
"""Revert mistaken camelCase/snake_case changes from fix_kernel_cpp_api.py."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "kernel" / "src"

FREE_FUNCTIONS: list[tuple[str, str]] = [
    ("evaluate_sphere_sphere_boolean", "EvaluateSphereSphereBoolean"),
    ("evaluate_sphere_box_boolean", "EvaluateSphereBoxBoolean"),
    ("evaluate_prism_box_boolean", "EvaluatePrismBoxBoolean"),
    ("make_composite_boolean_evaluator", "MakeCompositeBooleanEvaluator"),
    ("make_default_boolean_pipeline", "MakeDefaultBooleanPipeline"),
    ("make_default_fast_path_registry", "MakeDefaultFastPathRegistry"),
    ("probe_face_pair_intersections", "ProbeFacePairIntersections"),
    ("collect_face_pair_candidates", "CollectFacePairCandidates"),
    ("recognize_axis_aligned_box", "RecognizeAxisAlignedBox"),
    ("recognize_extrusion_prism", "RecognizeExtrusionPrism"),
    ("recognize_analytic_sphere", "RecognizeAnalyticSphere"),
    ("intersect_sphere_cylinder", "IntersectSphereCylinder"),
    ("intersect_sphere_sphere", "IntersectSphereSphere"),
    ("intersect_plane_cylinder", "IntersectPlaneCylinder"),
    ("intersect_plane_sphere", "IntersectPlaneSphere"),
    ("intersect_plane_plane", "IntersectPlanePlane"),
    ("classify_point_in_sphere", "ClassifyPointInSphere"),
    ("classify_point_in_prism", "ClassifyPointInPrism"),
    ("classify_point_in_box", "ClassifyPointInBox"),
    ("solid_class_to_face_region", "SolidClassToFaceRegion"),
    ("query_snap_candidates", "QuerySnapCandidates"),
    ("evaluate_box_boolean", "EvaluateBoxBoolean"),
    ("pipeline_stage_name", "PipelineStageName"),
    ("group_face_regions", "GroupFaceRegions"),
    ("unwrap_sphere_ring", "UnwrapSphereRing"),
    ("sample_edge_xyz", "SampleEdgeXyz"),
    ("select_csg_faces", "SelectCsgFaces"),
    ("extract_profile", "ExtractProfile"),
    ("sample_loop", "SampleLoop"),
    ("make_sphere", "MakeSphere"),
    ("make_box", "MakeBox"),
    ("tessellate_face", "TessellateFace"),
    ("validate_body", "ValidateBody"),
    ("dump_body", "DumpBody"),
]

MEMBER_FIELDS: dict[str, str] = {
    "surface": "Surface",
    "shells": "Shells",
    "faces": "Faces",
    "loops": "Loops",
    "curve": "Curve",
    "edge": "Edge",
    "sense": "Sense",
    "v0": "V0",
    "v1": "V1",
    "t0": "T0",
    "t1": "T1",
    "diagnostics": "Diagnostics",
    "mode": "Mode",
    "kinds": "Kinds",
    "tolerance": "Tolerance",
    "reference_point": "ReferencePoint",
    "near_point": "NearPoint",
    "body_guid": "BodyGuid",
    "quality": "Quality",
    "candidate_pairs": "CandidatePairs",
    "unsupported_pairs": "UnsupportedPairs",
    "attempted_intersects": "AttemptedIntersects",
    "nonempty_intersects": "NonemptyIntersects",
    "summary": "Summary",
    "points": "Points",
    "uv": "Uv",
    "xyz": "Xyz",
    "type": "Type",
    "outer": "Outer",
    "holes": "Holes",
    "linear_deflection": "LinearDeflection",
    "angular_deflection": "AngularDeflection",
    "min": "Min",
    "max": "Max",
    "center": "Center",
    "radius": "Radius",
    "name": "Name",
    "guid": "Guid",
    "point": "Point",
    "kind": "Kind",
    "value": "Value",
    "user_driven": "UserDriven",
    "distance": "Distance",
    "profile": "Profile",
    "plane": "Plane",
    "frame": "Frame",
    "constraints": "Constraints",
    "feature": "Feature",
    "feature_type": "FeatureType",
    "target_feature_id": "TargetFeatureId",
    "tool_feature_id": "ToolFeatureId",
    "output_body": "OutputBody",
    "target_face": "TargetFace",
    "vertices": "Vertices",
    "indices": "Indices",
    "position": "Position",
    "normal": "Normal",
    "issues": "Issues",
    "transform": "Transform",
    "albedo_path": "AlbedoPath",
    "entity_guid": "EntityGuid",
    "face_index": "FaceIndex",
    "bounds": "Bounds",
    "radial": "Radial",
    "partner": "Partner",
    "pcurve": "Pcurve",
    "closed": "Closed",
}

METHODS: dict[str, str] = {
    "outer_loop": "OuterLoop",
    "outer_loops": "OuterLoops",
    "inner_loops": "InnerLoops",
    "for_each_coedge": "ForEachCoedge",
    "param_of": "ParamOf",
    "param_at": "ParamAt",
    "position": "Position",
    "radius": "Radius",
    "length": "Length",
    "origin": "Origin",
    "direction": "Direction",
    "set_length": "SetLength",
    "set_xyz": "SetXyz",
    "xyz": "Xyz",
    "kind": "Kind",
    "eval": "Eval",
    "ok": "Ok",
    "surface_area": "SurfaceArea",
    "is_leaf": "IsLeaf",
    "collect_parameters": "CollectParameters",
    "set_suppressed": "SetSuppressed",
    "body_guid": "BodyGuid",
    "type_name": "TypeName",
    "display_name": "DisplayName",
    "find_by_name": "FindByName",
    "add_with_id": "AddWithId",
    "find_body": "FindBody",
    "features": "Features",
    "rebuild_sphere_body": "RebuildSphereBody",
    "rebuild_box_body": "RebuildBoxBody",
    "rebuild_extrude_body": "RebuildExtrudeBody",
    "rebuild_boolean_body": "RebuildBooleanBody",
    "set_last_regen_detail": "SetLastRegenDetail",
    "to_spec": "ToSpec",
    "create": "Create",
    "rebuild": "Rebuild",
    "to_string": "ToString",
}

CLASS_METHODS: list[tuple[str, str]] = [
    ("ObjectRegistry::add", "ObjectRegistry::Add"),
    ("ObjectRegistry::remove", "ObjectRegistry::Remove"),
    ("ObjectRegistry::clear", "ObjectRegistry::Clear"),
    ("ObjectRegistry::find", "ObjectRegistry::Find"),
    ("CompositeBooleanEvaluator::evaluate", "CompositeBooleanEvaluator::Evaluate"),
    ("StubBooleanEvaluator::evaluate", "StubBooleanEvaluator::Evaluate"),
    ("Guid::generate()", "Guid::Generate()"),
    ("Guid::nil()", "Guid::Nil()"),
    ("Guid::from_string", "Guid::FromString"),
    ("Guid::from_bytes", "Guid::FromBytes"),
]


def apply_member_fields(text: str) -> str:
    for old, new in MEMBER_FIELDS.items():
        text = re.sub(rf"\.{old}\b", f".{new}", text)
        text = re.sub(rf"->{old}\b", f"->{new}", text)
    return text


def apply_methods(text: str) -> str:
    for old, new in METHODS.items():
        text = re.sub(rf"\.{old}\(", f".{new}(", text)
        text = re.sub(rf"->{old}\(", f"->{new}(", text)
    return text


def apply_guid_validity(text: str) -> str:
    text = re.sub(r"if\s*\(\s*(\w+)\.is_nil\(\)\s*\)", r"if (!\1.IsValid())", text)
    text = re.sub(r"if\s*\(\s*!(\w+)\.is_nil\(\)\s*\)", r"if (\1.IsValid())", text)
    text = re.sub(r"(\w+)\.is_nil\(\)", r"!\1.IsValid()", text)
    return text


def fix_file(path: Path) -> bool:
    text = path.read_text(encoding="utf-8")
    original = text

    for old, new in FREE_FUNCTIONS:
        text = text.replace(old, new)

    for old, new in CLASS_METHODS:
        text = text.replace(old, new)

    text = apply_member_fields(text)
    text = apply_methods(text)
    text = apply_guid_validity(text)

    if text != original:
        path.write_text(text, encoding="utf-8")
        return True
    return False


def main() -> None:
    changed = 0
    for path in sorted(SRC.rglob("*.cpp")):
        if fix_file(path):
            print(f"reverted: {path.relative_to(ROOT)}")
            changed += 1
    print(f"done: {changed} file(s) reverted")


if __name__ == "__main__":
    main()
