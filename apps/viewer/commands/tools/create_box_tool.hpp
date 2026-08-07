#pragma once

#include "commands/itool.hpp"

#include "brep/guid.hpp"
#include "brep/math.hpp"

namespace brep::viewer::commands {

/// Two clicks on the y=0 plane define the box footprint; height is fixed.
class CreateBoxTool final : public ITool {
 public:
  [[nodiscard]] std::string_view id() const noexcept override {
    return "part.create_box";
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
  void commit_box(CommandContext& ctx, const Point3d& a, const Point3d& b);

  int step_{0};  // 0: wait first corner, 1: wait second
  Point3d corner_a_{};
  bool finished_{false};
  CommandResult result_{CommandResult::cancelled()};
};

}  // namespace brep::viewer::commands
