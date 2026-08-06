#pragma once

#include "ecs/world.hpp"
#include "vulkan_renderer.hpp"

#include <QVulkanWindow>

namespace brep::viewer {

class VulkanWindow final : public QVulkanWindow {
  Q_OBJECT
 public:
  explicit VulkanWindow(QWindow* parent = nullptr);

  void set_world(ecs::World* world) noexcept { world_ = world; }
  [[nodiscard]] ecs::World* world() noexcept { return world_; }

  [[nodiscard]] Camera& camera();
  [[nodiscard]] const Camera& camera() const;

  QVulkanWindowRenderer* createRenderer() override;
  bool eventFilter(QObject* watched, QEvent* event) override;

 protected:
  void mousePressEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

 private:
  void pointer_press(QPointF pos, Qt::MouseButton button);
  void pointer_move(QPointF pos, Qt::MouseButtons buttons);
  void pointer_release();
  void pointer_wheel(int angle_delta_y);
  void apply_key(int key);
  void sync_renderer();

  ecs::World* world_{nullptr};
  Camera fallback_camera_{};
  VulkanRenderer* renderer_{nullptr};
};

}  // namespace brep::viewer
