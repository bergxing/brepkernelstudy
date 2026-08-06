#include "document.hpp"

#include <QFileInfo>

namespace brep::viewer {

void DocumentSession::new_document(ecs::World& world,
                                   const std::string& wood_albedo_path) {
  world.create_demo_box_scene(wood_albedo_path);
  title_ = QStringLiteral("Untitled");
  path_.clear();
  dirty_ = false;
}

void DocumentSession::set_export_path(const QString& path) {
  path_ = path;
  title_ = QFileInfo(path).fileName();
  if (title_.isEmpty()) title_ = QStringLiteral("Untitled");
  dirty_ = false;
}

QString DocumentSession::window_title() const {
  QString name = title_.isEmpty() ? QStringLiteral("Untitled") : title_;
  if (dirty_) name += QLatin1Char('*');
  return name + QStringLiteral(" - B-Rep Kernel Viewer");
}

}  // namespace brep::viewer
