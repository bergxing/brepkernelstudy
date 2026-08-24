#!/usr/bin/env python3
"""Regenerate PascalCase viewer tool sources from UTF-8 snake_case backups."""
from __future__ import annotations

import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]

COMMON_REPLS = [
    ("ctx.clear_preview", "ctx.ClearPreview"),
    ("ctx.snap_session", "ctx.SnapSessionRef"),
    ("ctx.report_status", "ctx.ReportStatus"),
    ("ctx.set_preview_edges", "ctx.SetPreviewEdges"),
    ("ctx.set_preview", "ctx.SetPreview"),
    ("ctx.request_redraw", "ctx.RequestRedraw"),
    ("ctx.refresh_ui", "ctx.RefreshUi"),
    ("ctx.history", "ctx.History"),
    ("ctx.session", "ctx.Session"),
    ("ctx.world", "ctx.World"),
    ("ctx.view_camera", "ctx.ViewCamera"),
    ("ctx.viewport_w", "ctx.ViewportWidth"),
    ("ctx.viewport_h", "ctx.ViewportHeight"),
    ("ctx.wood_albedo_path", "ctx.WoodAlbedoPath"),
    ("CommandResult::cancelled", "CommandResult::Cancelled"),
    ("CommandResult::failed", "CommandResult::Failed"),
    ("CommandResult::ok", "CommandResult::Ok"),
    ("m_result.message", "m_result.Message"),
    ("make_wood_material", "MakeWoodMaterial"),
    ("guid.to_string()", "guid.ToString()"),
    ("mesh.vertices", "mesh.Vertices"),
    ("mesh.indices", "mesh.Indices"),
    ("make_sphere(model", "MakeSphere(model"),
    ("tessellate_body", "TessellateBody"),
    ("TessellationOptions::for_radius", "TessellationOptions::ForRadius"),
    (
        "scene.feature_id_for(Guid{}, body->guid)",
        "scene.feature_id_for(Guid{}, body->Guid)",
    ),
    ("if (!fid.is_nil())", "if (fid.IsValid())"),
    ("const Guid feature_guid = fid.guid;", "const Guid feature_guid = fid.Guid;"),
    ("const Guid guid = body->guid;", "const Guid guid = body->Guid;"),
    (".min =", ".Min ="),
    (".max =", ".Max ="),
    (".name =", ".Name ="),
    (".center =", ".Center ="),
    (".radius =", ".Radius ="),
    ("out.min", "out.Min"),
    ("out.max", "out.Max"),
    ("out.name", "out.Name"),
    ("spec.min", "spec.Min"),
    ("spec.max", "spec.Max"),
    ("object_for_body(body->guid)", "object_for_body(body->Guid)"),
    ("mesh_for_body(body->guid)", "mesh_for_body(body->Guid)"),
    (
        "create_body_renderable(body->name, body->guid,",
        "create_body_renderable(body->Name, body->Guid,",
    ),
]


def transform(src_path: pathlib.Path, dst_path: pathlib.Path, header: tuple[str, str]) -> None:
    src = src_path.read_text(encoding="utf-8")
    src = src.replace(header[0], header[1])
    for old, new in COMMON_REPLS:
        src = src.replace(old, new)
    dst_path.write_text(src, encoding="utf-8", newline="\n")
    print(f"wrote {dst_path}")


def main() -> None:
    tools = ROOT / "apps/viewer/commands/tools"
    transform(
        tools / "create_box_tool.cpp",
        tools / "CreateBoxTool.cpp",
        ("commands/tools/create_box_tool.h", "commands/tools/CreateBoxTool.h"),
    )
    transform(
        tools / "create_sphere_tool.cpp",
        tools / "CreateSphereTool.cpp",
        ("commands/tools/create_sphere_tool.h", "commands/tools/CreateSphereTool.h"),
    )
    transform(
        tools / "copy_tool.cpp",
        tools / "CopyTool.cpp",
        ("commands/tools/copy_tool.h", "commands/tools/CopyTool.h"),
    )


if __name__ == "__main__":
    main()
