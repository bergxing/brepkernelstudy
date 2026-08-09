#include "property_panel.hpp"

#include <QAbstractSpinBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include <optional>

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

  empty_label_ = new QLabel(tr("No selection"), this);
  empty_label_->setAlignment(Qt::AlignCenter);
  empty_label_->setStyleSheet(QStringLiteral("color: #888; padding: 16px;"));
  root->addWidget(empty_label_);

  form_host_ = new QWidget(this);
  auto* form_layout = new QVBoxLayout(form_host_);
  form_layout->setContentsMargins(0, 0, 0, 0);

  identity_group_ = new QGroupBox(tr("Object"), form_host_);
  auto* id_form = new QFormLayout(identity_group_);
  name_edit_ = new QLineEdit(identity_group_);
  name_edit_->setReadOnly(true);
  type_edit_ = new QLineEdit(identity_group_);
  type_edit_->setReadOnly(true);
  guid_edit_ = new QLineEdit(identity_group_);
  guid_edit_->setReadOnly(true);
  name_row_label_ = new QLabel(tr("Name"), identity_group_);
  type_row_label_ = new QLabel(tr("Type"), identity_group_);
  id_form->addRow(name_row_label_, name_edit_);
  id_form->addRow(type_row_label_, type_edit_);
  id_form->addRow(QStringLiteral("GUID"), guid_edit_);
  form_layout->addWidget(identity_group_);

  dims_group_ = new QGroupBox(tr("Dimensions (parameters)"), form_host_);
  auto* dim_form = new QFormLayout(dims_group_);
  length_spin_ = make_dim_spin(dims_group_, true);
  width_spin_ = make_dim_spin(dims_group_, true);
  height_spin_ = make_dim_spin(dims_group_, true);
  radius_spin_ = make_dim_spin(dims_group_, true);
  length_row_label_ = new QLabel(tr("Length (X)"), dims_group_);
  width_row_label_ = new QLabel(tr("Width (Z)"), dims_group_);
  height_row_label_ = new QLabel(tr("Height (Y)"), dims_group_);
  radius_row_label_ = new QLabel(tr("Radius"), dims_group_);
  dim_form->addRow(length_row_label_, length_spin_);
  dim_form->addRow(width_row_label_, width_spin_);
  dim_form->addRow(height_row_label_, height_spin_);
  dim_form->addRow(radius_row_label_, radius_spin_);
  dims_hint_ = new QLabel(dims_group_);
  dims_hint_->setStyleSheet(QStringLiteral("color:#888;"));
  dim_form->addRow(dims_hint_);
  form_layout->addWidget(dims_group_);
  form_layout->addStretch(1);

  root->addWidget(form_host_, 1);

  QObject::connect(length_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged),
                   this, [this](double) { on_dim_edited(); });
  QObject::connect(width_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged),
                   this, [this](double) { on_dim_edited(); });
  QObject::connect(height_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged),
                   this, [this](double) { on_dim_edited(); });
  QObject::connect(radius_spin_, qOverload<double>(&QDoubleSpinBox::valueChanged),
                   this, [this](double) { on_dim_edited(); });

  clear();
}

void PropertyPanel::retranslate_ui() {
  empty_label_->setText(tr("No selection"));
  identity_group_->setTitle(tr("Object"));
  name_row_label_->setText(tr("Name"));
  type_row_label_->setText(tr("Type"));
  dims_group_->setTitle(tr("Dimensions (parameters)"));
  length_row_label_->setText(tr("Length (X)"));
  width_row_label_->setText(tr("Width (Z)"));
  height_row_label_->setText(tr("Height (Y)"));
  radius_row_label_->setText(tr("Radius"));
  refresh_dim_hint();
}

void PropertyPanel::refresh_dim_hint() {
  if (box_params_visible_ || sphere_params_visible_) {
    dims_hint_->setText(
        tr("Parameter-driven · edits regenerate the model"));
  } else if (form_host_->isVisible()) {
    dims_hint_->setText(tr("No editable parameters"));
  } else {
    dims_hint_->clear();
  }
}

void PropertyPanel::changeEvent(QEvent* event) {
  if (event->type() == QEvent::LanguageChange) {
    retranslate_ui();
  }
  QWidget::changeEvent(event);
}

void PropertyPanel::block_dim_signals(bool block) {
  length_spin_->blockSignals(block);
  width_spin_->blockSignals(block);
  height_spin_->blockSignals(block);
  radius_spin_->blockSignals(block);
}

void PropertyPanel::set_box_mode(bool on) {
  length_row_label_->setVisible(on);
  width_row_label_->setVisible(on);
  height_row_label_->setVisible(on);
  length_spin_->setVisible(on);
  width_spin_->setVisible(on);
  height_spin_->setVisible(on);
  length_spin_->setEnabled(on);
  width_spin_->setEnabled(on);
  height_spin_->setEnabled(on);
}

void PropertyPanel::set_sphere_mode(bool on) {
  radius_row_label_->setVisible(on);
  radius_spin_->setVisible(on);
  radius_spin_->setEnabled(on);
}

void PropertyPanel::set_enabled(bool enabled) {
  empty_label_->setVisible(!enabled);
  form_host_->setVisible(enabled);
}

void PropertyPanel::clear() {
  set_enabled(false);
  current_feature_ = {};
  box_params_visible_ = false;
  sphere_params_visible_ = false;
  name_edit_->clear();
  type_edit_->clear();
  guid_edit_->clear();
  block_dim_signals(true);
  length_spin_->setValue(0.0);
  width_spin_->setValue(0.0);
  height_spin_->setValue(0.0);
  radius_spin_->setValue(0.0);
  block_dim_signals(false);
  set_box_mode(false);
  set_sphere_mode(false);
  refresh_dim_hint();
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
    name_edit_->setText(tr("(unnamed)"));
  }

  current_feature_ = {};
  std::optional<adapter::SceneObject> obj;
  if (adapter_) {
    if (const auto* fref = registry.try_get<ecs::FeatureRef>(entity)) {
      current_feature_ = feat::FeatureId{fref->feature_guid};
      obj = adapter_->object_for_feature(current_feature_);
    } else if (const auto* body = registry.try_get<ecs::BodyRef>(entity)) {
      obj = adapter_->object_for_body(body->guid);
      if (obj) current_feature_ = feat::FeatureId{obj->feature_guid};
    }
  }

  const bool is_box = obj && obj->box.has_value();
  const bool is_sphere = obj && obj->sphere.has_value();
  if (is_box) {
    type_edit_->setText(QStringLiteral("BoxFeature"));
  } else if (is_sphere) {
    type_edit_->setText(QStringLiteral("SphereFeature"));
  } else {
    type_edit_->setText(registry.all_of<ecs::BodyRef>(entity)
                            ? QStringLiteral("Body")
                            : QStringLiteral("Renderable"));
  }

  if (const auto* body = registry.try_get<ecs::BodyRef>(entity)) {
    guid_edit_->setText(QString::fromStdString(body->guid.to_string()));
  } else {
    guid_edit_->clear();
  }

  block_dim_signals(true);
  set_box_mode(is_box);
  set_sphere_mode(is_sphere);
  box_params_visible_ = is_box;
  sphere_params_visible_ = is_sphere;
  if (is_box) {
    length_spin_->setValue(obj->box->length);
    width_spin_->setValue(obj->box->width);
    height_spin_->setValue(obj->box->height);
  } else if (is_sphere) {
    radius_spin_->setValue(obj->sphere->radius);
  }
  block_dim_signals(false);
  refresh_dim_hint();
}

void PropertyPanel::on_dim_edited() {
  if (updating_ui_ || !adapter_ || current_feature_.is_nil()) return;

  updating_ui_ = true;
  if (box_params_visible_) {
    adapter_->set_box_params(current_feature_,
                             adapter::BoxParams{.length = length_spin_->value(),
                                                .width = width_spin_->value(),
                                                .height = height_spin_->value()});
  } else if (sphere_params_visible_) {
    adapter_->set_sphere_params(
        current_feature_,
        adapter::SphereParams{.radius = radius_spin_->value()});
  }
  updating_ui_ = false;

  if (on_params_changed_) on_params_changed_(current_feature_);
}

}  // namespace brep::viewer
