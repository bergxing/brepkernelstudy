#pragma once

#include "ecs/world.hpp"

#include <QString>

namespace brep::viewer {

/// Lightweight document session (title / path / dirty) for the viewer.
class DocumentSession {
 public:
  [[nodiscard]] const QString& title() const noexcept { return title_; }
  [[nodiscard]] const QString& path() const noexcept { return path_; }
  [[nodiscard]] bool dirty() const noexcept { return dirty_; }

  void mark_dirty() noexcept { dirty_ = true; }
  void mark_clean() noexcept { dirty_ = false; }

  /// Reset session metadata and rebuild the demo scene.
  void new_document(ecs::World& world, const std::string& wood_albedo_path);

  void set_export_path(const QString& path);

  [[nodiscard]] QString window_title() const;

 private:
  QString title_{QStringLiteral("Untitled")};
  QString path_;
  bool dirty_{false};
};

}  // namespace brep::viewer
