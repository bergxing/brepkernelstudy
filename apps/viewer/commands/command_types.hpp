#pragma once

#include "document.hpp"
#include "ecs/world.hpp"

#include "api/mesh.hpp"

#include <QString>
#include <QWidget>

#include <functional>
#include <string>

namespace brep::viewer {
class Camera;
class VulkanWindow;
}  // namespace brep::viewer

namespace brep::viewer::commands {

class DocumentHistory;

enum class CommandStatus {
  Ok,
  Cancelled,
  Failed,
};

enum class CommandKind {
  Instant,
  Interactive,
  View,
};

struct CommandResult {
  CommandStatus status{CommandStatus::Ok};
  QString message;

  [[nodiscard]] static CommandResult ok(QString msg = {}) {
    return {CommandStatus::Ok, std::move(msg)};
  }
  [[nodiscard]] static CommandResult cancelled(QString msg = {}) {
    return {CommandStatus::Cancelled, std::move(msg)};
  }
  [[nodiscard]] static CommandResult failed(QString msg) {
    return {CommandStatus::Failed, std::move(msg)};
  }

  [[nodiscard]] bool succeeded() const noexcept {
    return status == CommandStatus::Ok;
  }
};

struct CommandContext {
  ecs::World* world{nullptr};
  DocumentSession* session{nullptr};
  DocumentHistory* history{nullptr};
  QWidget* parent_widget{nullptr};
  Camera* view_camera{nullptr};
  VulkanWindow* viewport{nullptr};
  std::string wood_albedo_path;
  int viewport_w{1};
  int viewport_h{1};

  std::function<void(const QString&)> report_status;
  std::function<void()> request_redraw;
  std::function<void()> after_document_reset;
  std::function<void()> refresh_ui;
  std::function<void(EdgeMesh)> set_preview_edges;
  /// Wire + optional translucent solid fill for interactive tool previews.
  std::function<void(EdgeMesh, TriangleMesh)> set_preview;
  std::function<void()> clear_preview;
};

}  // namespace brep::viewer::commands
