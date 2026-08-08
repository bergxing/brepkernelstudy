#include "property_panel.hpp"

#include "brep/math.hpp"

#include <QAbstractSpinBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace brep::viewer {
namespace {

struct Aabb {
  Point3d min{};
  Point3d max{};
  bool valid{false};
};

Aabb mesh_aabb(const TriangleMesh& mesh, const Point3d& offset) {
  Aabb box;
  if (mesh.vertices.empty()) return box;

  box.min = Point3d{mesh.vertices.front().position.x() + offset.x(),
                    mesh.vertices.front().position.y() + offset.y(),
                    mesh.vertices.front().position.z() + offset.z()};
  box.max = box.min;
  for (const auto& v : mesh.vertices) {
    const double x = v.position.x() + offset.x();
    const double y = v.position.y() + offset.y();
    const double z = v.position.z() + offset.z();
    box.min.x() = std::min(box.min.x(), x);
    box.min.y() = std::min(box.min.y(), y);
    box.min.z() = std::min(box.min.z(), z);
    box.max.x() = std::max(box.max.x(), x);
    box.max.y() = std::max(box.max.y(), y);
    box.max.z() = std::max(box.max.z(), z);
  }
  box.valid = true;
  return box;
}

QDoubleSpinBox* make_dim_spin(QWidget* parent) {
  auto* spin = new QDoubleSpinBox(parent);
  spin->setDecimals(4);
  spin->setRange(0.0, 1.0e9);
  spin->setSingleStep(0.1);
  spin->setSuffix(QStringLiteral(""));
  spin->setReadOnly(true);
  spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
  spin->setAlignment(Qt::AlignRight);
  return spin;
}

}  // namespace

PropertyPanel::PropertyPanel(QWidget* parent) : QWidget(parent) {
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(8);

  empty_label_ = new QLabel(QStringLiteral("未选择对象"), this);
  empty_label_->setAlignment(Qt::AlignCenter);
  empty_label_->setStyleSheet(QStringLiteral("color: #888; padding: 16px;"));
  root->addWidget(empty_label_);

  form_host_ = new QWidget(this);
  auto* form_layout = new QVBoxLayout(form_host_);
  form_layout->setContentsMargins(0, 0, 0, 0);

  auto* identity = new QGroupBox(QStringLiteral("对象"), form_host_);
  auto* id_form = new QFormLayout(identity);
  name_edit_ = new QLineEdit(identity);
  name_edit_->setReadOnly(true);
  type_edit_ = new QLineEdit(identity);
  type_edit_->setReadOnly(true);
  guid_edit_ = new QLineEdit(identity);
  guid_edit_->setReadOnly(true);
  id_form->addRow(QStringLiteral("名称"), name_edit_);
  id_form->addRow(QStringLiteral("类型"), type_edit_);
  id_form->addRow(QStringLiteral("GUID"), guid_edit_);
  form_layout->addWidget(identity);

  auto* dims = new QGroupBox(QStringLiteral("尺寸 (AABB)"), form_host_);
  auto* dim_form = new QFormLayout(dims);
  length_spin_ = make_dim_spin(dims);
  width_spin_ = make_dim_spin(dims);
  height_spin_ = make_dim_spin(dims);
  dim_form->addRow(QStringLiteral("长 (X)"), length_spin_);
  dim_form->addRow(QStringLiteral("宽 (Z)"), width_spin_);
  dim_form->addRow(QStringLiteral("高 (Y)"), height_spin_);
  form_layout->addWidget(dims);
  form_layout->addStretch(1);

  root->addWidget(form_host_, 1);
  clear();
}

void PropertyPanel::set_enabled(bool enabled) {
  empty_label_->setVisible(!enabled);
  form_host_->setVisible(enabled);
}

void PropertyPanel::clear() {
  set_enabled(false);
  name_edit_->clear();
  type_edit_->clear();
  guid_edit_->clear();
  length_spin_->setValue(0.0);
  width_spin_->setValue(0.0);
  height_spin_->setValue(0.0);
}

void PropertyPanel::show_entity(entt::registry& registry, entt::entity entity) {
  if (entity == entt::null || !registry.valid(entity)) {
    clear();
    return;
  }

  set_enabled(true);

  if (const auto* name = registry.try_get<ecs::Name>(entity)) {
    name_edit_->setText(QString::fromStdString(name->value));
  } else {
    name_edit_->setText(QStringLiteral("(unnamed)"));
  }

  type_edit_->setText(registry.all_of<ecs::BodyRef>(entity)
                          ? QStringLiteral("长方体 / Body")
                          : QStringLiteral("Renderable"));

  if (const auto* body = registry.try_get<ecs::BodyRef>(entity)) {
    guid_edit_->setText(QString::fromStdString(body->guid.to_string()));
  } else {
    guid_edit_->clear();
  }

  const auto* mesh = registry.try_get<ecs::MeshComponent>(entity);
  const auto* xform = registry.try_get<ecs::Transform>(entity);
  if (!mesh) {
    length_spin_->setValue(0.0);
    width_spin_->setValue(0.0);
    height_spin_->setValue(0.0);
    return;
  }

  const Point3d offset = xform ? xform->position : Point3d{};
  const Aabb box = mesh_aabb(mesh->triangles, offset);
  if (!box.valid) {
    length_spin_->setValue(0.0);
    width_spin_->setValue(0.0);
    height_spin_->setValue(0.0);
    return;
  }

  // Ground-plane boxes: length along X, width along Z, height along Y.
  length_spin_->setValue(std::abs(box.max.x() - box.min.x()));
  width_spin_->setValue(std::abs(box.max.z() - box.min.z()));
  height_spin_->setValue(std::abs(box.max.y() - box.min.y()));
}

}  // namespace brep::viewer
