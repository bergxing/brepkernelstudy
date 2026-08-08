#include "property_panel.hpp"

#include "brep/feat/box_feature.hpp"
#include "brep/math.hpp"

#include <QAbstractSpinBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include <cmath>

namespace brep::viewer {
namespace {

QDoubleSpinBox* make_dim_spin(QWidget* parent, bool editable) {
  auto* spin = new QDoubleSpinBox(parent);
  spin->setDecimals(4);
  spin->setRange(1.0e-4, 1.0e9);
  spin->setSingleStep(0.1);
  spin->setAlignment(Qt::AlignRight);
  if (!editable) {
    spin->setReadOnly(true);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
  }
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

  auto* dims = new QGroupBox(QStringLiteral("尺寸 (参数)"), form_host_);
  auto* dim_form = new QFormLayout(dims);
  length_spin_ = make_dim_spin(dims, true);
  width_spin_ = make_dim_spin(dims, true);
  height_spin_ = make_dim_spin(dims, true);
  dim_form->addRow(QStringLiteral("长 (X)"), length_spin_);
  dim_form->addRow(QStringLiteral("宽 (Z)"), width_spin_);
  dim_form->addRow(QStringLiteral("高 (Y)"), height_spin_);
  dims_hint_ = new QLabel(QStringLiteral("修改后自动再生"), dims);
  dims_hint_->setStyleSheet(QStringLiteral("color:#888;"));
  dim_form->addRow(dims_hint_);
  form_layout->addWidget(dims);
  form_layout->addStretch(1);

  root->addWidget(form_host_, 1);

  QObject::connect(length_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged),
                   this, [this](double) { on_dim_edited(); });
  QObject::connect(width_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged),
                   this, [this](double) { on_dim_edited(); });
  QObject::connect(height_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged),
                   this, [this](double) { on_dim_edited(); });

  clear();
}

void PropertyPanel::block_dim_signals(bool block) {
  length_spin_->blockSignals(block);
  width_spin_->blockSignals(block);
  height_spin_->blockSignals(block);
}

void PropertyPanel::set_enabled(bool enabled) {
  empty_label_->setVisible(!enabled);
  form_host_->setVisible(enabled);
}

void PropertyPanel::clear() {
  set_enabled(false);
  current_feature_ = {};
  name_edit_->clear();
  type_edit_->clear();
  guid_edit_->clear();
  block_dim_signals(true);
  length_spin_->setValue(0.0);
  width_spin_->setValue(0.0);
  height_spin_->setValue(0.0);
  block_dim_signals(false);
  length_spin_->setEnabled(false);
  width_spin_->setEnabled(false);
  height_spin_->setEnabled(false);
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

  current_feature_ = {};
  const feat::BoxFeature* box = nullptr;
  if (part_) {
    if (const auto* fref = registry.try_get<ecs::FeatureRef>(entity)) {
      current_feature_ = feat::FeatureId{fref->feature_guid};
      if (auto* f = part_->features().find(current_feature_)) {
        if (f->type_name() == "Box") {
          box = static_cast<const feat::BoxFeature*>(f);
        }
      }
    } else if (const auto* body = registry.try_get<ecs::BodyRef>(entity)) {
      if (auto* f = part_->features().find_by_body(body->guid)) {
        current_feature_ = f->id();
        if (f->type_name() == "Box") {
          box = static_cast<const feat::BoxFeature*>(f);
        }
      }
    }
  }

  type_edit_->setText(box ? QStringLiteral("BoxFeature")
                          : (registry.all_of<ecs::BodyRef>(entity)
                                 ? QStringLiteral("Body")
                                 : QStringLiteral("Renderable")));

  if (const auto* body = registry.try_get<ecs::BodyRef>(entity)) {
    guid_edit_->setText(QString::fromStdString(body->guid.to_string()));
  } else {
    guid_edit_->clear();
  }

  block_dim_signals(true);
  if (box) {
    const auto& params = part_->parameters();
    length_spin_->setValue(params.get(box->length_id()).value_or(0.0));
    width_spin_->setValue(params.get(box->width_id()).value_or(0.0));
    height_spin_->setValue(params.get(box->height_id()).value_or(0.0));
    length_spin_->setEnabled(true);
    width_spin_->setEnabled(true);
    height_spin_->setEnabled(true);
    dims_hint_->setText(QStringLiteral("参数驱动 · 修改后自动再生"));
  } else {
    length_spin_->setValue(0.0);
    width_spin_->setValue(0.0);
    height_spin_->setValue(0.0);
    length_spin_->setEnabled(false);
    width_spin_->setEnabled(false);
    height_spin_->setEnabled(false);
    dims_hint_->setText(QStringLiteral("无 Box 参数"));
  }
  block_dim_signals(false);
}

void PropertyPanel::on_dim_edited() {
  if (updating_ui_ || !part_ || current_feature_.is_nil()) return;
  auto* f = part_->features().find(current_feature_);
  if (!f || f->type_name() != "Box") return;

  updating_ui_ = true;
  part_->edit_feature_params(current_feature_,
                             {{"Length", length_spin_->value()},
                              {"Width", width_spin_->value()},
                              {"Height", height_spin_->value()}});
  updating_ui_ = false;

  if (on_params_changed_) on_params_changed_(current_feature_);
}

}  // namespace brep::viewer
