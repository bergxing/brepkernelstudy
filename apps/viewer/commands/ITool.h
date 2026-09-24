#pragma once

#include "commands/CommandTypes.h"

#include <string_view>

namespace brep::viewer::commands
{

/// Interactive tool (Phase 3): multi-step mouse/keyboard until finish/cancel.
class ITool
{
 public:
  virtual ~ITool() = default;

  [[nodiscard]] virtual std::string_view Id() const noexcept = 0;
  [[nodiscard]] virtual QString Prompt() const = 0;

  virtual void OnStart(CommandContext& ctx) = 0;
  virtual bool OnMousePress(CommandContext& ctx, float x, float y,
                            int button) = 0;
  virtual void OnMouseMove(CommandContext& ctx, float x, float y) = 0;
  /// Optional keyboard handling (e.g. Space to confirm selection).
  /// `key` uses Qt::Key values. Return true if consumed.
  virtual bool OnKeyPress(CommandContext& ctx, int key)
  {
    (void)ctx;
    (void)key;
    return false;
  }
  /// Viewport right-click (no drag). Return true if the tool handled it.
  virtual bool OnContextMenu(CommandContext& ctx, float x, float y)
  {
    (void)ctx;
    (void)x;
    (void)y;
    return false;
  }
  virtual void OnCancel(CommandContext& ctx) = 0;

  /// When true, Ctrl+Z / Ctrl+Y stay inside the tool (last pick), not
  /// document history or property edits.
  [[nodiscard]] virtual bool OwnsUndoRedo() const noexcept
  {
    return false;
  }
  [[nodiscard]] virtual bool CanUndoStep() const noexcept
  {
    return false;
  }
  [[nodiscard]] virtual bool CanRedoStep() const noexcept
  {
    return false;
  }
  virtual bool UndoStep(CommandContext& ctx)
  {
    (void)ctx;
    return false;
  }
  virtual bool RedoStep(CommandContext& ctx)
  {
    (void)ctx;
    return false;
  }

  /// When true, viewport keeps normal select / box-select / Ctrl+select.
  [[nodiscard]] virtual bool AllowsViewportSelection() const noexcept
  {
    return false;
  }

  [[nodiscard]] virtual bool IsFinished() const noexcept = 0;
  [[nodiscard]] virtual CommandResult Result() const = 0;
};

}  // namespace brep::viewer::commands
