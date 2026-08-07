#pragma once

#include "document.hpp"
#include "ecs/world.hpp"

#include <QString>
#include <QWidget>

#include <functional>
#include <string>
#include <string_view>

namespace brep::viewer::commands {

enum class CommandStatus {
  Ok,
  Cancelled,
  Failed,
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

/// Runtime services a command may use. UI fills this; commands stay UI-thin.
struct CommandContext {
  ecs::World* world{nullptr};
  DocumentSession* session{nullptr};
  QWidget* parent_widget{nullptr};
  std::string wood_albedo_path;

  /// Optional: status bar / log toast.
  std::function<void(const QString&)> report_status;
  /// Optional: request viewport redraw after model changes.
  std::function<void()> request_redraw;
  /// Optional: rebind overlays (ViewCube camera pointer, etc.).
  std::function<void()> after_document_reset;
};

/// Instant command: one execute() call (Phase 1 — no interactive tools yet).
class ICommand {
 public:
  virtual ~ICommand() = default;

  /// Stable registry id, e.g. "doc.new".
  [[nodiscard]] virtual std::string_view id() const noexcept = 0;
  [[nodiscard]] virtual std::string_view title() const noexcept = 0;

  [[nodiscard]] virtual bool can_execute(const CommandContext& ctx) const {
    return ctx.world != nullptr;
  }

  virtual CommandResult execute(CommandContext& ctx) = 0;
};

}  // namespace brep::viewer::commands
