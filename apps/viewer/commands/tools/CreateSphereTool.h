#pragma once

#include "commands/ITool.h"

#include "api/Core.h"

namespace brep::viewer::commands
{

/// Two-point sphere: center (mesh or ground) â†?radius point.
class CreateSphereTool final : public ITool
{
 public:
  [[nodiscard]] std::string_view id() const noexcept override
  {
    return "part.create_sphere";
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
  /// Mesh surface first; else y=0 ground.
  bool pick_point(CommandContext& ctx, float x, float y, Point3d& hit) const;
  void update_preview(CommandContext& ctx, float x, float y);
  void clear_preview(CommandContext& ctx);
  void commit_sphere(CommandContext& ctx, double radius);

  int m_step{0};  // 0: center, 1: radius
  Point3d m_center{};
  bool m_finished{false};
  CommandResult m_result{CommandResult::Cancelled()};
};

}  // namespace brep::viewer::commands
