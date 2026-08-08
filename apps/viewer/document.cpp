#include "document.hpp"

#include <QFileInfo>

namespace brep::viewer {

void DocumentSession::sync_title_from_world(const ecs::World& world) {
  if (const auto* doc = world.document()) {
    title_ = QString::fromStdString(doc->name);
  } else {
    title_ = QStringLiteral("Untitled");
  }
}

void DocumentSession::new_blank_document(ecs::World& world) {
  world.create_blank_scene();
  sync_title_from_world(world);
  path_.clear();
  dirty_ = false;
}

void DocumentSession::new_demo_document(ecs::World& world,
                                       const std::string& wood_albedo_path) {
  world.create_demo_box_scene(wood_albedo_path);
  sync_title_from_world(world);
  path_.clear();
  dirty_ = false;
}

void DocumentSession::set_export_path(const QString& path) {
  path_ = path;
  title_ = QFileInfo(path).fileName();
  if (title_.isEmpty()) title_ = QStringLiteral("Untitled");
  dirty_ = false;
}

void DocumentSession::set_document_path(const QString& path) {
  set_export_path(path);
}

QString DocumentSession::window_title() const {
  QString name = title_.isEmpty() ? QStringLiteral("Untitled") : title_;
  if (dirty_) name += QLatin1Char('*');
  return name + QStringLiteral(" - XCAD");
}

}  // namespace brep::viewer
