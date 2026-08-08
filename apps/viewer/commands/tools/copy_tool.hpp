#pragma once

#include "commands/itool.hpp"

#include "brep/builder.hpp"
#include "brep/math.hpp"

#include <vector>

namespace brep::viewer::commands {

/// Copy selected boxes: pick base point → place point → translated copies.
class CopyTool final : public ITool {
 public:
  [[nodiscard]] std::string_view id() const noexcept override {
    return "edit.copy";
  }
  [[nodiscard]] QString prompt() const override;

  void on_start(CommandContext& ctx) override;
  bool on_mouse_press(CommandContext& ctx, float x, float y, int button) override;
  void on_mouse_move(CommandContext& ctx, float x, float y) override;
  void on_cancel(CommandContext& ctx) override;

  [[nodiscard]] bool is_finished() const noexcept override { return finished_; }
  [[nodiscard]] CommandResult result() const override { return result_; }

 private:
  bool pick_ground(CommandContext& ctx, float x, float y, Point3d& hit) const;
  void update_preview(CommandContext& ctx, float x, float y);
  void clear_preview(CommandContext& ctx);
  void commit_copies(CommandContext& ctx, const Point3d& place);

  int step_{0};  // 0: base, 1: place
  Point3d base_{};
  std::vector<BoxSpec> sources_;
  bool finished_{false};
  CommandResult result_{CommandResult::cancelled()};
};

}  // namespace brep::viewer::commands
