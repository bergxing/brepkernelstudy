#pragma once

#include "commands/command_registry.hpp"

#include <QDialog>

class QListWidget;
class QLineEdit;

namespace brep::viewer::commands {

/// Simple Ctrl+Shift+P command launcher (Phase 3).
class CommandPalette final : public QDialog {
  Q_OBJECT
 public:
  explicit CommandPalette(const CommandRegistry& registry,
                          QWidget* parent = nullptr);

  [[nodiscard]] QString selected_command_id() const;

 private:
  void rebuild_list(const QString& filter);

  const CommandRegistry& registry_;
  QLineEdit* filter_{nullptr};
  QListWidget* list_{nullptr};
};

}  // namespace brep::viewer::commands
