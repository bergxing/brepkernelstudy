#pragma once

#include "adapter/SceneAdapter.h"
#include "ecs/Components.h"

#include "api/Modeling.h"

#include <QWidget>

#include <entt/entt.hpp>

#include <functional>
#include <optional>

class QGroupBox;
class QLabel;
class QLineEdit;
class QDoubleSpinBox;
class QEvent;

namespace brep::viewer
{

/// Right-dock properties: box L/W/H or sphere Radius via SceneAdapter.
class PropertyPanel final : public QWidget
{
  Q_OBJECT
 public:
  explicit PropertyPanel(QWidget* parent = nullptr);

  void set_adapter(adapter::SceneAdapter* adapter)
  {
      m_adapter = adapter; 
  }

  using ParamsChangedFn = std::function<void(brep::feat::FeatureId)>;
  void set_params_changed_callback(ParamsChangedFn cb)
  {
    m_onParamsChanged = std::move(cb);
  }

  void clear();
  void show_entity(entt::registry& registry, entt::entity entity);
  void retranslate_ui();

 protected:
  void changeEvent(QEvent* event) override;

 private:
  void set_enabled(bool enabled);
  void refresh_dim_hint();
  void on_dim_edited();
  void block_dim_signals(bool block);
  void set_box_mode(bool on);
  void set_sphere_mode(bool on);

  adapter::SceneAdapter* m_adapter{nullptr};
  ParamsChangedFn m_onParamsChanged;
  brep::feat::FeatureId m_currentFeature{};
  bool m_updatingUi{false};

  QLineEdit* m_nameEdit{nullptr};
  QLineEdit* m_typeEdit{nullptr};
  QLineEdit* m_guidEdit{nullptr};
  QDoubleSpinBox* m_lengthSpin{nullptr};  // X / unused for sphere
  QDoubleSpinBox* m_widthSpin{nullptr};   // Z
  QDoubleSpinBox* m_heightSpin{nullptr};  // Y
  QDoubleSpinBox* m_radiusSpin{nullptr};
  QLabel* m_emptyLabel{nullptr};
  QLabel* m_dimsHint{nullptr};
  QWidget* m_formHost{nullptr};
  QGroupBox* m_identityGroup{nullptr};
  QGroupBox* m_dimsGroup{nullptr};
  QLabel* m_nameRowLabel{nullptr};
  QLabel* m_typeRowLabel{nullptr};
  QLabel* m_lengthRowLabel{nullptr};
  QLabel* m_widthRowLabel{nullptr};
  QLabel* m_heightRowLabel{nullptr};
  QLabel* m_radiusRowLabel{nullptr};
  bool m_boxParamsVisible{false};
  bool m_sphereParamsVisible{false};
  std::optional<brep::boolean::BooleanOp> m_booleanOp{};
};

}  // namespace brep::viewer
