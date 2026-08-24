#!/usr/bin/env python3
"""Fix broken method renames from batch 15b."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIXES = [
    ("Setupmenus", "SetupMenus"),
    ("Setupwindow_menu", "SetupWindowMenu"),
    ("Setuplanguage_menu", "SetupLanguageMenu"),
    ("Setuptoolbar", "SetupToolbar"),
    ("Setupview_toolbar", "SetupViewToolbar"),
    ("Setupproperty_dock", "SetupPropertyDock"),
    ("Setupcursor_tip", "SetupCursorTip"),
    ("handle_tool_mouse", "HandleToolMouse"),
    ("wire_vulkan_window", "WireVulkanWindow"),
    ("create_renderable", "CreateRenderable"),
    ("Createblank_scene", "CreateBlankScene"),
    ("Createdemo_box_scene", "CreateDemoBoxScene"),
    ("adopt_document", "AdoptDocument"),
    ("main_camera()", "MainCamera()"),
    ("main_camera(", "MainCamera("),
    ("log_format", "LogFormat"),
]


def main() -> None:
    changed = 0
    for path in ROOT.rglob("*"):
        if path.suffix not in {".cpp", ".h", ".hpp"}:
            continue
        if "third_party" in path.parts:
            continue
        text = path.read_text(encoding="utf-8")
        updated = text
        for old, new in FIXES:
            updated = updated.replace(old, new)
        if updated != text:
            path.write_text(updated, encoding="utf-8")
            changed += 1
            print(path.relative_to(ROOT))
    print(f"Fixed {changed} files.")


if __name__ == "__main__":
    main()
