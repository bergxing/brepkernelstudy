#!/usr/bin/env python3
"""Update viewer commands layer call sites (ICommand, ITool, CommandManager)."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {"third_party", "cmake-build-mingw-debug", "build-kernel", "build-mingw", ".git", "build"}

ICOMMAND_OVERRIDES = [
    (r"\bstd::string_view id\(\)", "std::string_view Id()"),
    (r"\bstd::string_view title\(\)", "std::string_view Title()"),
    (r"\bCommandKind kind\(\)", "CommandKind Kind()"),
    (r"\bbool can_execute\(", "bool CanExecute("),
    (r"\bCommandResult execute\(", "CommandResult Execute("),
    (r"\bstd::unique_ptr<ITool> make_tool\(", "std::unique_ptr<ITool> MakeTool("),
]

ITOOL_OVERRIDES = [
    (r"\bQString prompt\(\)", "QString Prompt()"),
    (r"\bvoid on_start\(", "void OnStart("),
    (r"\bbool on_mouse_press\(", "bool OnMousePress("),
    (r"\bvoid on_mouse_move\(", "void OnMouseMove("),
    (r"\bbool on_key_press\(", "bool OnKeyPress("),
    (r"\bvoid on_cancel\(", "void OnCancel("),
    (r"\bbool allows_viewport_selection\(", "bool AllowsViewportSelection("),
    (r"\bbool is_finished\(", "bool IsFinished("),
    (r"\bCommandResult result\(\)", "CommandResult Result()"),
]

REPLACEMENTS = [
    ("CommandResult::ok(", "CommandResult::Ok("),
    ("CommandResult::cancelled(", "CommandResult::Cancelled("),
    ("CommandResult::failed(", "CommandResult::Failed("),
    ("register_builtin_commands", "RegisterBuiltinCommands"),
    ("open_xl_file", "OpenXlFile"),
    ("CommandRegistry::register_command", "CommandRegistry::RegisterCommand"),
    ("->can_execute(", "->CanExecute("),
    ("->execute(", "->Execute("),
    ("->make_tool(", "->MakeTool("),
    ("->kind()", "->Kind()"),
    ("->id()", "->Id()"),
    ("->title()", "->Title()"),
    ("->prompt()", "->Prompt()"),
    ("->on_start(", "->OnStart("),
    ("->on_mouse_press(", "->OnMousePress("),
    ("->on_mouse_move(", "->OnMouseMove("),
    ("->on_key_press(", "->OnKeyPress("),
    ("->on_cancel(", "->OnCancel("),
    ("->allows_viewport_selection()", "->AllowsViewportSelection()"),
    ("->is_finished()", "->IsFinished()"),
    ("->result()", "->Result()"),
    ("command_manager_.history()", "command_manager_.History()"),
    ("command_manager_.has_active_tool()", "command_manager_.HasActiveTool()"),
    ("command_manager_.active_prompt()", "command_manager_.ActivePrompt()"),
    (
        "command_manager_.active_tool_allows_selection()",
        "command_manager_.ActiveToolAllowsSelection()",
    ),
    ("command_manager_.cancel_active_tool(", "command_manager_.CancelActiveTool("),
    ("command_manager_.tool_mouse_press(", "command_manager_.ToolMousePress("),
    ("command_manager_.tool_mouse_move(", "command_manager_.ToolMouseMove("),
    ("command_manager_.tool_key_press(", "command_manager_.ToolKeyPress("),
    ("command_manager_.run(", "command_manager_.Run("),
    ("registry_.ids()", "registry_.Ids()"),
    ("registry_.create(", "registry_.Create("),
    ("registry_.contains(", "registry_.Contains("),
    ("registry_.register_command(", "registry_.RegisterCommand("),
    (".succeeded()", ".Succeeded()"),
    ("history->clear()", "history->Clear()"),
    ("ctx.history->clear()", "ctx.history->Clear()"),
    ("fi.Path()", "fi.path()"),
]


def should_process(path: Path) -> bool:
    if path.suffix not in {".cpp", ".h", ".hpp"}:
        return False
    if "apps" not in path.parts or "viewer" not in path.parts:
        return False
    return not any(skip in path.parts for skip in SKIP_DIRS)


def process_file(path: Path) -> bool:
    original = path.read_text(encoding="utf-8")
    updated = original
    for pattern, repl in ICOMMAND_OVERRIDES:
        updated = re.sub(pattern, repl, updated)
    for pattern, repl in ITOOL_OVERRIDES:
        updated = re.sub(pattern, repl, updated)
    for old, new in REPLACEMENTS:
        updated = updated.replace(old, new)
    # entt registry uses create(), not Create().
    if "ecs" in path.parts:
        updated = updated.replace("registry_.Create(", "registry_.create(")
    # Tool method definitions missed by header-only regex pass.
    for old, new in [
        ("::prompt() const", "::Prompt() const"),
        ("::on_start(", "::OnStart("),
        ("::on_mouse_press(", "::OnMousePress("),
        ("::on_mouse_move(", "::OnMouseMove("),
        ("::on_key_press(", "::OnKeyPress("),
        ("::on_cancel(", "::OnCancel("),
    ]:
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
