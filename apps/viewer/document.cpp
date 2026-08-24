#include "Document.h"

#include <QFileInfo>

namespace brep::viewer
{

void DocumentSession::sync_title_from_world(const ecs::World& world)
{
  if (const auto* doc = world.document())
{
    m_title = QString::fromStdString(doc->Name);
  }
  else
{
    m_title = QStringLiteral("Untitled");
  }
}

void DocumentSession::new_blank_document(ecs::World& world)
{
  world.create_blank_scene();
  sync_title_from_world(world);
  m_path.clear();
  m_dirty = false;
}

void DocumentSession::new_demo_document(ecs::World& world,
                                       const std::string& wood_albedo_path)
{
  world.create_demo_box_scene(wood_albedo_path);
  sync_title_from_world(world);
  m_path.clear();
  m_dirty = false;
}

void DocumentSession::set_export_path(const QString& path)
{
  m_path = path;
  m_title = QFileInfo(path).fileName();
  if (m_title.isEmpty()) m_title = QStringLiteral("Untitled");
  m_dirty = false;
}

void DocumentSession::set_document_path(const QString& path)
{
  set_export_path(path);
}

QString DocumentSession::window_title() const
{
  QString name = m_title.isEmpty() ? QStringLiteral("Untitled") : m_title;
  if (m_dirty) name += QLatin1Char('*');
  return name + QStringLiteral(" - XCAD");
}

}  // namespace brep::viewer
