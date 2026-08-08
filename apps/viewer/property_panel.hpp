#pragma once

#include "ecs/components.hpp"

#include "brep/feat/feature.hpp"
#include "brep/part.hpp"

#include <QWidget>

#include <entt/entt.hpp>

#include <functional>

class QGroupBox;
class QLabel;
class QLineEdit;
class QDoubleSpinBox;
class QEvent;

namespace brep::viewer {

/// Right-dock properties view: editable box L/W/H when a BoxFeature is selected.
class PropertyPanel final : public QWidget {
  Q_OBJECT
 public:
  explicit PropertyPanel(QWidget* parent = nullptr);

  void set_part(brep::Part* part) { part_ = part; }

  using ParamsChangedFn = std::function<void(brep::feat::FeatureId)>;
  void set_params_changed_callback(ParamsChangedFn cb) {
    on_params_changed_ = std::move(cb);
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

  brep::Part* part_{nullptr};
  ParamsChangedFn on_params_changed_;
  brep::feat::FeatureId current_feature_{};
  bool updating_ui_{false};

  QLineEdit* name_edit_{nullptr};
  QLineEdit* type_edit_{nullptr};
  QLineEdit* guid_edit_{nullptr};
  QDoubleSpinBox* length_spin_{nullptr};  // X
  QDoubleSpinBox* width_spin_{nullptr};   // Z
  QDoubleSpinBox* height_spin_{nullptr};  // Y
  QLabel* empty_label_{nullptr};
  QLabel* dims_hint_{nullptr};
  QWidget* form_host_{nullptr};
  QGroupBox* identity_group_{nullptr};
  QGroupBox* dims_group_{nullptr};
  QLabel* name_row_label_{nullptr};
  QLabel* type_row_label_{nullptr};
  QLabel* length_row_label_{nullptr};
  QLabel* width_row_label_{nullptr};
  QLabel* height_row_label_{nullptr};
  bool box_params_visible_{false};
};

}  // namespace brep::viewer
