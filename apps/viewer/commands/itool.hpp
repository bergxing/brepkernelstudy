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
  virtual void on_cancel(CommandContext& ctx) = 0;

  [[nodiscard]] virtual bool is_finished() const noexcept = 0;
  [[nodiscard]] virtual CommandResult result() const = 0;
};

}  // namespace brep::viewer::commands
