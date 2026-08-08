#pragma once

#include "commands/command_types.hpp"

#include <string_view>

namespace brep::viewer::commands {

/// Interactive tool (Phase 3): multi-step mouse/keyboard until finish/cancel.
class ITool {
 public:
  virtual ~ITool() = default;

  [[nodiscard]] virtual std::string_view id() const noexcept = 0;
  [[nodiscard]] virtual QString prompt() const = 0;

  virtual void on_start(CommandContext& ctx) = 0;
  virtual bool on_mouse_press(CommandContext& ctx, float x, float y,
                              int button) = 0;
  virtual void on_mouse_move(CommandContext& ctx, float x, float y) = 0;
  /// Optional keyboard handling (e.g. Space to confirm selection).
  /// `key` uses Qt::Key values. Return true if consumed.
  virtual bool on_key_press(CommandContext& ctx, int key) {
    (void)ctx;
    (void)key;
    return false;
  }
  virtual void on_cancel(CommandContext& ctx) = 0;

  /// When true, viewport keeps normal select / box-select / Ctrl+select.
  [[nodiscard]] virtual bool allows_viewport_selection() const noexcept {
    return false;
  }

  [[nodiscard]] virtual bool is_finished() const noexcept = 0;
  [[nodiscard]] virtual CommandResult result() const = 0;
};

}  // namespace brep::viewer::commands
