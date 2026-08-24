#include "PropertyPanel.h"

#include <QAbstractSpinBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include <optional>

namespace brep::viewer
{
namespace
{

QDoubleSpinBox* make_dim_spin(QWidget* parent, bool editable)
{
  auto* spin = new QDoubleSpinBox(parent);
  spin->setDecimals(4);
  spin->setRange(1.0e-4, 1.0e9);
  spin->setSingleStep(0.1);
  spin->setAlignment(Qt::AlignRight);
  if (!editable)
  {
    spin->setReadOnly(true);
    spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
  }
  return spin;
}

}  // namespace

PropertyPanel::PropertyPanel(QWidget* parent) : QWidget(parent)
{
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(8);

  m_emptyLabel = new QLabel(tr("No selection"), this);
  m_emptyLabel->setAlignment(Qt::AlignCenter);
  m_emptyLabel->setStyleSheet(QStringLiteral("color: #888; padding: 16px;"));
  root->addWidget(m_emptyLabel);

  m_formHost = new QWidget(this);
  auto* form_layout = new QVBoxLayout(m_formHost);
  form_layout->setContentsMargins(0, 0, 0, 0);

  m_identityGroup = new QGroupBox(tr("Object"), m_formHost);
  auto* id_form = new QFormLayout(m_identityGroup);
  m_nameEdit = new QLineEdit(m_identityGroup);
  m_nameEdit->setReadOnly(true);
  m_typeEdit = new QLineEdit(m_identityGroup);
  m_typeEdit->setReadOnly(true);
  m_guidEdit = new QLineEdit(m_identityGroup);
  m_guidEdit->setReadOnly(true);
  m_nameRowLabel = new QLabel(tr("Name"), m_identityGroup);
  m_typeRowLabel = new QLabel(tr("Type"), m_identityGroup);
  id_form->addRow(m_nameRowLabel, m_nameEdit);
  id_form->addRow(m_typeRowLabel, m_typeEdit);
  id_form->addRow(QStringLiteral("GUID"), m_guidEdit);
  form_layout->addWidget(m_identityGroup);

  m_dimsGroup = new QGroupBox(tr("Dimensions (parameters)"), m_formHost);
  auto* dim_form = new QFormLayout(m_dimsGroup);
  m_lengthSpin = make_dim_spin(m_dimsGroup, true);
  m_widthSpin = make_dim_spin(m_dimsGroup, true);
  m_heightSpin = make_dim_spin(m_dimsGroup, true);
  m_radiusSpin = make_dim_spin(m_dimsGroup, true);
  m_lengthRowLabel = new QLabel(tr("Length (X)"), m_dimsGroup);
  m_widthRowLabel = new QLabel(tr("Width (Z)"), m_dimsGroup);
  m_heightRowLabel = new QLabel(tr("Height (Y)"), m_dimsGroup);
  m_radiusRowLabel = new QLabel(tr("Radius"), m_dimsGroup);
  dim_form->addRow(m_lengthRowLabel, m_lengthSpin);
  dim_form->addRow(m_widthRowLabel, m_widthSpin);
  dim_form->addRow(m_heightRowLabel, m_heightSpin);
  dim_form->addRow(m_radiusRowLabel, m_radiusSpin);
  m_dimsHint = new QLabel(m_dimsGroup);
  m_dimsHint->setStyleSheet(QStringLiteral("color:#888;"));
  dim_form->addRow(m_dimsHint);
  form_layout->addWidget(m_dimsGroup);
  form_layout->addStretch(1);

  root->addWidget(m_formHost, 1);

  QObject::connect(m_lengthSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                   this, [this](double)
  {
                       on_dim_edited(); 
                   });
  QObject::connect(m_widthSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                   this, [this](double)
  {
                       on_dim_edited(); 
                   });
  QObject::connect(m_heightSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                   this, [this](double)
  {
                       on_dim_edited(); 
                   });
  QObject::connect(m_radiusSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                   this, [this](double)
  {
                       on_dim_edited(); 
                   });

  clear();
}

void PropertyPanel::retranslate_ui()
{
  m_emptyLabel->setText(tr("No selection"));
  m_identityGroup->setTitle(tr("Object"));
  m_nameRowLabel->setText(tr("Name"));
  m_typeRowLabel->setText(tr("Type"));
  m_dimsGroup->setTitle(tr("Dimensions (parameters)"));
  m_lengthRowLabel->setText(tr("Length (X)"));
  m_widthRowLabel->setText(tr("Width (Z)"));
  m_heightRowLabel->setText(tr("Height (Y)"));
  m_radiusRowLabel->setText(tr("Radius"));
  refresh_dim_hint();
}

void PropertyPanel::refresh_dim_hint()
{
  if (m_boxParamsVisible || m_sphereParamsVisible)
{
    m_dimsHint->setText(
        tr("Parameter-driven · edits regenerate the model"));
  } else if (m_booleanOp.has_value())
  {
    switch (*m_booleanOp)
  {
      case boolean::BooleanOp::Union:
        m_dimsHint->setText(tr("Operation: Union (Fuse)"));
        break;
      case boolean::BooleanOp::Subtract:
        m_dimsHint->setText(tr("Operation: Subtract (Cut)"));
        break;
      case boolean::BooleanOp::Intersect:
        m_dimsHint->setText(tr("Operation: Intersect (Common)"));
        break;
    }
  } else if (m_formHost->isVisible())
  {
    m_dimsHint->setText(tr("No editable parameters"));
  }
  else
  {
    m_dimsHint->clear();
  }
}

void PropertyPanel::changeEvent(QEvent* event)
{
  if (event->type() == QEvent::LanguageChange)
{
    retranslate_ui();
  }
  QWidget::changeEvent(event);
}

void PropertyPanel::block_dim_signals(bool block)
{
  m_lengthSpin->blockSignals(block);
  m_widthSpin->blockSignals(block);
  m_heightSpin->blockSignals(block);
  m_radiusSpin->blockSignals(block);
}

void PropertyPanel::set_box_mode(bool on)
{
  m_lengthRowLabel->setVisible(on);
  m_widthRowLabel->setVisible(on);
  m_heightRowLabel->setVisible(on);
  m_lengthSpin->setVisible(on);
  m_widthSpin->setVisible(on);
  m_heightSpin->setVisible(on);
  m_lengthSpin->setEnabled(on);
  m_widthSpin->setEnabled(on);
  m_heightSpin->setEnabled(on);
}

void PropertyPanel::set_sphere_mode(bool on)
{
  m_radiusRowLabel->setVisible(on);
  m_radiusSpin->setVisible(on);
  m_radiusSpin->setEnabled(on);
}

void PropertyPanel::set_enabled(bool enabled)
{
  m_emptyLabel->setVisible(!enabled);
  m_formHost->setVisible(enabled);
}

void PropertyPanel::clear()
{
  set_enabled(false);
  m_currentFeature = {};
  m_boxParamsVisible = false;
  m_sphereParamsVisible = false;
  m_booleanOp.reset();
  m_nameEdit->clear();
  m_typeEdit->clear();
  m_guidEdit->clear();
  block_dim_signals(true);
  m_lengthSpin->setValue(0.0);
  m_widthSpin->setValue(0.0);
  m_heightSpin->setValue(0.0);
  m_radiusSpin->setValue(0.0);
  block_dim_signals(false);
  set_box_mode(false);
  set_sphere_mode(false);
  refresh_dim_hint();
}

void PropertyPanel::show_entity(entt::registry& registry, entt::entity entity)
{
  if (entity == entt::null || !registry.valid(entity))
{
    clear();
    return;
  }

  set_enabled(true);

  if (const auto* name = registry.try_get<ecs::Name>(entity))
  {
    m_nameEdit->setText(QString::fromStdString(name->value));
  }
  else
  {
    m_nameEdit->setText(tr("(unnamed)"));
  }

  m_currentFeature = {};
  std::optional<adapter::SceneObject> obj;
  if (m_adapter)
  {
    if (const auto* fref = registry.try_get<ecs::FeatureRef>(entity))
  {
      m_currentFeature = feat::FeatureId{fref->feature_guid};
      obj = m_adapter->object_for_feature(m_currentFeature);
    } else if (const auto* body = registry.try_get<ecs::BodyRef>(entity))
    {
      obj = m_adapter->object_for_body(body->guid);
      if (obj) m_currentFeature = feat::FeatureId{obj->feature_guid};
    }
  }

  const bool is_box = obj && obj->box.has_value();
  const bool is_sphere = obj && obj->sphere.has_value();
  const bool is_boolean = obj && obj->boolean_info.has_value();
  if (is_box)
  {
    m_typeEdit->setText(QStringLiteral("BoxFeature"));
  } else if (is_sphere)
  {
    m_typeEdit->setText(QStringLiteral("SphereFeature"));
  } else if (is_boolean)
  {
    m_typeEdit->setText(QStringLiteral("BooleanFeature"));
  }
  else
  {
    m_typeEdit->setText(registry.all_of<ecs::BodyRef>(entity)
                            ? QStringLiteral("Body")
                            : QStringLiteral("Renderable"));
  }

  if (const auto* body = registry.try_get<ecs::BodyRef>(entity))
  {
    m_guidEdit->setText(QString::fromStdString(body->guid.ToString()));
  }
  else
  {
    m_guidEdit->clear();
  }

  block_dim_signals(true);
  set_box_mode(is_box);
  set_sphere_mode(is_sphere);
  m_boxParamsVisible = is_box;
  m_sphereParamsVisible = is_sphere;
  m_booleanOp.reset();
  if (is_box)
  {
    m_lengthSpin->setValue(obj->box->length);
    m_widthSpin->setValue(obj->box->width);
    m_heightSpin->setValue(obj->box->height);
  } else if (is_sphere)
  {
    m_radiusSpin->setValue(obj->sphere->radius);
  } else if (is_boolean)
  {
    m_booleanOp = obj->boolean_info->op;
  }
  block_dim_signals(false);
  refresh_dim_hint();
}

void PropertyPanel::on_dim_edited()
{
  if (m_updatingUi || !m_adapter || !m_currentFeature.IsValid()) return;

  m_updatingUi = true;
  if (m_boxParamsVisible)
  {
    m_adapter->set_box_params(m_currentFeature,
                             adapter::BoxParams{.length = m_lengthSpin->value(),
                                                .width = m_widthSpin->value(),
                                                .height = m_heightSpin->value()});
  } else if (m_sphereParamsVisible)
  {
    m_adapter->set_sphere_params(
        m_currentFeature,
        adapter::SphereParams{.radius = m_radiusSpin->value()});
  }
  m_updatingUi = false;

  if (m_onParamsChanged) m_onParamsChanged(m_currentFeature);
}

}  // namespace brep::viewer
