#pragma once

#include "ecs/components.hpp"

#include <QWidget>

#include <entt/entt.hpp>

class QLabel;
class QLineEdit;
class QDoubleSpinBox;

namespace brep::viewer {

/// Right-dock properties view for the current selection (box L/W/H for now).
class PropertyPanel final : public QWidget {
  Q_OBJECT
 public:
  explicit PropertyPanel(QWidget* parent = nullptr);

  void clear();
  void show_entity(entt::registry& registry, entt::entity entity);

 private:
  void set_enabled(bool enabled);

  QLineEdit* name_edit_{nullptr};
  QLineEdit* type_edit_{nullptr};
  QLineEdit* guid_edit_{nullptr};
  QDoubleSpinBox* length_spin_{nullptr};  // X
  QDoubleSpinBox* width_spin_{nullptr};   // Z
  QDoubleSpinBox* height_spin_{nullptr};  // Y
  QLabel* empty_label_{nullptr};
  QWidget* form_host_{nullptr};
};

}  // namespace brep::viewer
