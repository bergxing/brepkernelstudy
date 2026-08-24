#!/usr/bin/env python3
"""Align MainWindow and related viewer UI .cpp with PascalCase headers."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "apps" / "viewer"

REPLACEMENTS = [
    ("MainWindow::refresh_edit_actions", "MainWindow::RefreshEditActions"),
    ("MainWindow::sync_language_menu_checks", "MainWindow::SyncLanguageMenuChecks"),
    ("MainWindow::refresh_view_titles", "MainWindow::RefreshViewTitles"),
    ("MainWindow::apply_wheel_zoom", "MainWindow::ApplyWheelZoom"),
    ("MainWindow::apply_standard_view", "MainWindow::ApplyStandardView"),
    ("MainWindow::sync_tool_ui", "MainWindow::SyncToolUi"),
    ("MainWindow::update_property_panel", "MainWindow::UpdatePropertyPanel"),
    ("MainWindow::set_snap_enabled", "MainWindow::SetSnapEnabled"),
    ("MainWindow::sync_snap_action", "MainWindow::SyncSnapAction"),
    ("MainWindow::hide_cursor_tip", "MainWindow::HideCursorTip"),
    ("MainWindow::update_cursor_tip_at_global", "MainWindow::UpdateCursorTipAtGlobal"),
    ("MainWindow::clear_view_fill_states", "MainWindow::ClearViewFillStates"),
    ("MainWindow::on_RunCommand", "MainWindow::OnRunCommand"),
    ("MainWindow::on_command_palette", "MainWindow::OnCommandPalette"),
    ("MainWindow::on_sub_window_activated", "MainWindow::OnSubWindowActivated"),
    ("MainWindow::on_new_view", "MainWindow::OnNewView"),
    ("MainWindow::on_quad_views", "MainWindow::OnQuadViews"),
    ("MainWindow::on_tile_views", "MainWindow::OnTileViews"),
    ("MainWindow::on_cascade_views", "MainWindow::OnCascadeViews"),
    ("MainWindow::on_close_active_view", "MainWindow::OnCloseActiveView"),
    ("&MainWindow::on_sub_window_activated", "&MainWindow::OnSubWindowActivated"),
    ("&MainWindow::on_RunCommand", "&MainWindow::OnRunCommand"),
    ("&MainWindow::on_command_palette", "&MainWindow::OnCommandPalette"),
    ("&MainWindow::on_new_view", "&MainWindow::OnNewView"),
    ("&MainWindow::on_quad_views", "&MainWindow::OnQuadViews"),
    ("&MainWindow::on_tile_views", "&MainWindow::OnTileViews"),
    ("&MainWindow::on_cascade_views", "&MainWindow::OnCascadeViews"),
    ("&MainWindow::on_close_active_view", "&MainWindow::OnCloseActiveView"),
    ("&MainWindow::set_snap_enabled", "&MainWindow::SetSnapEnabled"),
    ("refresh_edit_actions()", "RefreshEditActions()"),
    ("sync_language_menu_checks()", "SyncLanguageMenuChecks()"),
    ("refresh_view_titles()", "RefreshViewTitles()"),
    ("apply_wheel_zoom(", "ApplyWheelZoom("),
    ("apply_standard_view(", "ApplyStandardView("),
    ("sync_tool_ui()", "SyncToolUi()"),
    ("update_property_panel(", "UpdatePropertyPanel("),
    ("set_snap_enabled(", "SetSnapEnabled("),
    ("sync_snap_action()", "SyncSnapAction()"),
    ("hide_cursor_tip()", "HideCursorTip()"),
    ("update_cursor_tip_at_global(", "UpdateCursorTipAtGlobal("),
    ("clear_view_fill_states()", "ClearViewFillStates()"),
    ("PropertyPanel::set_box_mode", "PropertyPanel::Setbox_mode"),
    ("PropertyPanel::set_sphere_mode", "PropertyPanel::Setsphere_mode"),
    ("PropertyPanel::set_enabled", "PropertyPanel::Setenabled"),
    ("ViewMdiSubWindow::set_view_caption", "ViewMdiSubWindow::Setview_caption"),
    ("ViewMdiSubWindow::apply_fill_geometry", "ViewMdiSubWindow::Applyfill_geometry"),
    ("ViewMdiSubWindow::clear_fill", "ViewMdiSubWindow::Clearfill"),
    ("->set_view_caption(", "->Setview_caption("),
    ("->clear_fill()", "->Clearfill()"),
    ("->fill_workspace()", "->fill_workspace()"),  # unchanged, already correct
]

FILES = list(ROOT.rglob("*.cpp"))


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
    changed = [p for p in FILES if patch_file(p)]
    print(f"Patched {len(changed)} files")
    for p in changed:
        print(f"  {p.relative_to(ROOT.parent.parent)}")


if __name__ == "__main__":
    main()
