#pragma once

#include "commands/ITool.h"

#include "api/Core.h"

namespace brep::viewer::commands
{

/// Three-point box: base corner â†?opposite corner â†?height.
class CreateBoxTool final : public ITool
{
 public:
  [[nodiscard]] std::string_view id() const noexcept override
  {
    return "part.create_box";
  }
  [[nodiscard]] QString prompt() const override;

  void on_start(CommandContext& ctx) override;
  bool on_mouse_press(CommandContext& ctx, float x, float y, int button) override;
  void on_mouse_move(CommandContext& ctx, float x, float y) override;
  void on_cancel(CommandContext& ctx) override;

  [[nodiscard]] bool is_finished() const noexcept override
  {
      return m_finished; 
  }
  [[nodiscard]] CommandResult result() const override
  {
      return m_result; 
  }

 private:
  bool pick_ground(CommandContext& ctx, float x, float y, Point3d& hit) const;
  bool pick_height(CommandContext& ctx, float x, float y, double& height) const;
  void update_preview(CommandContext& ctx, float x, float y);
  void clear_preview(CommandContext& ctx);
  void commit_box(CommandContext& ctx, double height);

  int m_step{0};  // 0: first corner, 1: opposite corner, 2: height
  Point3d m_cornerA{};
  Point3d m_cornerB{};
  bool m_finished{false};
  CommandResult m_result{CommandResult::Cancelled()};
};

}  // namespace brep::viewer::commands
