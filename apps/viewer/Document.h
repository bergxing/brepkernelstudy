#pragma once

#include "ecs/World.h"

#include <QString>

namespace brep::viewer
{

/// Lightweight document session (title / path / dirty) for the viewer.
class DocumentSession
{
 public:
  [[nodiscard]] const QString& title() const noexcept
  {
      return m_title; 
  }
  [[nodiscard]] const QString& path() const noexcept
  {
      return m_path; 
  }
  [[nodiscard]] bool dirty() const noexcept
  {
      return m_dirty; 
  }

  void mark_dirty() noexcept
  {
      m_dirty = true; 
  }
  void mark_clean() noexcept
  {
      m_dirty = false; 
  }

  /// New blank Document → Part (no geometry).
  void new_blank_document(ecs::World& world);

  /// Reset session metadata and rebuild the demo scene.
  void new_demo_document(ecs::World& world, const std::string& wood_albedo_path);

  void set_export_path(const QString& path);

  /// Bind session to a saved/opened document path and clear dirty.
  void set_document_path(const QString& path);

  [[nodiscard]] QString window_title() const;

 private:
  void sync_title_from_world(const ecs::World& world);

  QString m_title{QStringLiteral("Untitled")};
  QString m_path;
  bool m_dirty{false};
};

}  // namespace brep::viewer
