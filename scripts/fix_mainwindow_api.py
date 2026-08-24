#!/usr/bin/env python3
"""Restore proper PascalCase for MainWindow and fix include paths."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

FIXES = [
    # includes
    ('"commands/Commandmanager.h"', '"commands/CommandManager.h"'),
    ('"commands/Commandregistry.h"', '"commands/CommandRegistry.h"'),
    ('"commands/snap/Snapsettings.h"', '"commands/snap/SnapSettings.h"'),
    ('"Propertypanel.h"', '"PropertyPanel.h"'),
    ('"Viewcube.h"', '"ViewCube.h"'),
    ('"Vulkanwindow.h"', '"VulkanWindow.h"'),
    # broken batch renames -> proper PascalCase
    ("Onrun_command", "OnRunCommand"),
    ("Oncommand_palette", "OnCommandPalette"),
    ("Onsub_window_activated", "OnSubWindowActivated"),
    ("Onnew_view", "OnNewView"),
    ("Onquad_views", "OnQuadViews"),
    ("Ontile_views", "OnTileViews"),
    ("Oncascade_views", "OnCascadeViews"),
    ("Onclose_active_view", "OnCloseActiveView"),
    ("Clearview_fill_states", "ClearViewFillStates"),
    ("Wirevulkan_window", "WireVulkanWindow"),
    ("show_SnapSettingsPtr", "ShowSnapSettings"),
    ("Setsnap_enabled", "SetSnapEnabled"),
    ("save_SnapSettingsPtr", "SaveSnapSettings"),
    ("Syncsnap_action", "SyncSnapAction"),
    ("Synclanguage_menu_checks", "SyncLanguageMenuChecks"),
    ("Refreshview_titles", "RefreshViewTitles"),
    ("Applywheel_zoom", "ApplyWheelZoom"),
    ("Applystandard_view", "ApplyStandardView"),
    ("Refreshwindow_title", "RefreshWindowTitle"),
    ("Refreshedit_actions", "RefreshEditActions"),
    ("Synctool_ui", "SyncToolUi"),
    ("Updateproperty_panel", "UpdatePropertyPanel"),
    ("Hidecursor_tip", "HideCursorTip"),
    ("Updatecursor_tip_at_global", "UpdateCursorTipAtGlobal"),
    ("open_document", "OpenDocument"),
    ("create_view_window", "CreateViewWindow"),
    ("active_vulkan_window", "ActiveVulkanWindow"),
    ("active_viewport_container", "ActiveViewportContainer"),
    ("vulkan_window_for_sub", "VulkanWindowForSub"),
    ("vulkan_window_at_global", "VulkanWindowAtGlobal"),
    ("request_all_views_update", "RequestAllViewsUpdate"),
    ("ensure_minimum_view", "EnsureMinimumView"),
    ("handle_snap_key", "HandleSnapKey"),
    ("retranslate_ui", "RetranslateUi"),
    ("title_for_standard_view", "TitleForStandardView"),
    ("format_view_title", "FormatViewTitle"),
    ("place_view_cube", "PlaceViewCube"),
    ("is_view_layout_object", "IsViewLayoutObject"),
    ("rebind_view_cube_camera", "RebindViewCubeCamera"),
    ("bind_action", "BindAction"),
    ("run_command", "RunCommand"),
    ("confirm_close_or_save", "ConfirmCloseOrSave"),
    ("make_command_context", "MakeCommandContext"),
    ("show_viewport_context_menu", "ShowViewportContextMenu"),
    ("resolve_cursor_tip_text", "ResolveCursorTipText"),
    ("map_global_to_viewport", "MapGlobalToViewport"),
    ("setup_menus", "SetupMenus"),
    ("setup_window_menu", "SetupWindowMenu"),
    ("setup_language_menu", "SetupLanguageMenu"),
    ("setup_toolbar", "SetupToolbar"),
    ("setup_view_toolbar", "SetupViewToolbar"),
    ("setup_property_dock", "SetupPropertyDock"),
    ("setup_cursor_tip", "SetupCursorTip"),
    ("SnapSettingsPtr_", "m_snapSettings"),
    ("SnapSessionPtr_", "m_snapSession"),
    ("SnapSettingsPtr.", "m_snapSettings."),
    ("SnapSessionPtr.", "m_snapSession."),
    ("&SnapSettingsPtr_", "&m_snapSettings"),
    ("&SnapSessionPtr_", "&m_snapSession"),
]


def main() -> None:
    changed = 0
    for path in ROOT.rglob("*"):
        if path.suffix not in {".cpp", ".h", ".hpp"}:
            continue
        if "third_party" in path.parts:
            continue
        if "apps" not in path.parts or "viewer" not in path.parts:
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
