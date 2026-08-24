#pragma once

#include "commands/ITool.h"

#include "api/Core.h"
#include "api/Modeling.h"

#include <vector>

namespace brep::viewer::commands
{

/// Copy boxes with two entry paths:
/// - pre-select then Copy â†?base â†?place
/// - Copy then select â†?Space â†?base â†?place
class CopyTool final : public ITool
{
 public:
  [[nodiscard]] std::string_view id() const noexcept override
  {
    return "edit.copy";
  }
  [[nodiscard]] QString prompt() const override;

  void on_start(CommandContext& ctx) override;
  bool on_mouse_press(CommandContext& ctx, float x, float y, int button) override;
  void on_mouse_move(CommandContext& ctx, float x, float y) override;
  bool on_key_press(CommandContext& ctx, int key) override;
  void on_cancel(CommandContext& ctx) override;

  [[nodiscard]] bool allows_viewport_selection() const noexcept override
  {
    return m_step == 0;
  }

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
  bool confirm_selection(CommandContext& ctx);
  void update_preview(CommandContext& ctx, float x, float y);
  void clear_preview(CommandContext& ctx);
  void commit_copies(CommandContext& ctx, const Point3d& place);

  int m_step{0};  // 0: select objects, 1: base, 2: place
  Point3d m_base{};
  std::vector<BoxSpec> m_sources;
  bool m_finished{false};
  CommandResult m_result{CommandResult::Cancelled()};
};

}  // namespace brep::viewer::commands
