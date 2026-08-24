#!/usr/bin/env python3
"""Regenerate BuiltinCommands.cpp from UTF-8 backup with PascalCase API."""
from __future__ import annotations

import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC = ROOT / "apps/viewer/commands/builtin_commands.cpp"
DST = ROOT / "apps/viewer/commands/BuiltinCommands.cpp"


def main() -> None:
    src = SRC.read_text(encoding="utf-8")

    src = src.replace(
        "commands/tools/create_box_tool.h", "commands/tools/CreateBoxTool.h"
    )
    src = src.replace(
        "commands/tools/create_sphere_tool.h", "commands/tools/CreateSphereTool.h"
    )
    src = src.replace("commands/tools/copy_tool.h", "commands/tools/CopyTool.h")

    for old, new in [
        ("ctx.world", "ctx.World"),
        ("ctx.session", "ctx.Session"),
        ("ctx.history", "ctx.History"),
        ("ctx.parent_widget", "ctx.ParentWidget"),
        ("ctx.report_status", "ctx.ReportStatus"),
        ("ctx.request_redraw", "ctx.RequestRedraw"),
        ("ctx.after_document_reset", "ctx.AfterDocumentReset"),
        ("ctx.refresh_ui", "ctx.RefreshUi"),
        ("ctx.wood_albedo_path", "ctx.WoodAlbedoPath"),
    ]:
        src = src.replace(old, new)

    src = src.replace("CommandResult::cancelled", "CommandResult::Cancelled")
    src = src.replace("CommandResult::failed", "CommandResult::Failed")
    src = src.replace("CommandResult::ok", "CommandResult::Ok")
    src = src.replace("->main_part()", "->MainPart()")
    src = src.replace(
        "object_for_body(body->guid)", "object_for_body(body->Guid)"
    )
    src = src.replace("mesh_for_body(body->guid)", "mesh_for_body(body->Guid)")
    src = src.replace(
        "create_body_renderable(body->name, body->guid,",
        "create_body_renderable(body->Name, body->Guid,",
    )
    src = src.replace("body->guid.to_string()", "body->Guid.ToString()")
    src = src.replace(".min =", ".Min =")
    src = src.replace(".max =", ".Max =")
    src = src.replace(".name =", ".Name =")
    src = src.replace("return a.guid < b.guid;", "return a.Guid < b.Guid;")
    src = src.replace("loaded.document->guid", "loaded.document->Guid")
    src = src.replace("result->name", "result->Name")
    src = src.replace("make_wood_material", "MakeWoodMaterial")
    src = src.replace("->set_path(", "->SetPath(")
    src = src.replace("->mark_clean()", "->MarkClean()")

    src = src.replace(
        """    if (path.isEmpty() ||
        !path.endsWith(QStringLiteral(".xl"), Qt::CaseInsensitive)
    {
    {
      QString start = path;""",
        """    if (path.isEmpty() ||
        !path.endsWith(QStringLiteral(".xl"), Qt::CaseInsensitive))
    {
      QString start = path;""",
    )
    src = src.replace(
        """      if (!path.endsWith(QStringLiteral(".xl"), Qt::CaseInsensitive)
      {
      {
        path += QStringLiteral(".xl");
      }
    }""",
        """      if (!path.endsWith(QStringLiteral(".xl"), Qt::CaseInsensitive))
      {
        path += QStringLiteral(".xl");
      }
    }""",
    )
    src = src.replace(
        """    if (!path.endsWith(QStringLiteral(".dxf"), Qt::CaseInsensitive
    {
        )
    {
      path += QStringLiteral(".dxf");
    }""",
        """    if (!path.endsWith(QStringLiteral(".dxf"), Qt::CaseInsensitive))
    {
      path += QStringLiteral(".dxf");
    }""",
    )

    DST.write_text(src, encoding="utf-8", newline="\n")
    print(f"wrote {DST} ({DST.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
