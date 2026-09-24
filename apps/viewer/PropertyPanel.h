#pragma once

#include "adapter/ISceneService.h"
#include "ecs/Components.h"

#include "api/Modeling.h"

#include <QWidget>

#include <entt/entt.hpp>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

class QFormLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QDoubleSpinBox;
class QEvent;
class QSlider;

namespace brep::viewer
{

/// Right-dock properties: identity + PropertySheet, edits via SetPrimitive.
class PropertyPanel final : public QWidget
{
    Q_OBJECT

public:
    explicit PropertyPanel(QWidget* parent = nullptr);

    void SetAdapter(adapter::ISceneService* adapter)
    {
        m_adapter = adapter;
    }

    using ParamsChangedFn = std::function<void(brep::feat::FeatureId)>;
    void SetParamsChangedCallback(ParamsChangedFn cb)
    {
        m_onParamsChanged = std::move(cb);
    }

    void clear();
    void ShowEntity(entt::registry& registry, entt::entity entity);
    void retranslate_ui();

protected:
    void changeEvent(QEvent* event) override;

private:
    struct FieldRow
    {
        std::string Id;
        QLabel* Label{nullptr};
        QDoubleSpinBox* Spin{nullptr};
        QSlider* Slider{nullptr};
    };

    void SetEnabled(bool enabled);
    void ClearParamRows();
    void UpdateParamRowValues(const PropertySheet& sheet);
    void RebuildParamRows(const PropertySheet& sheet);
    QWidget* BuildFieldRow(const PropertyField& field);
    void OnFieldEdited(std::string_view fieldId);
    void SyncSliderFromSpin(FieldRow& row);
    void SyncSpinFromSlider(FieldRow& row);

    adapter::ISceneService* m_adapter{nullptr};
    ParamsChangedFn m_onParamsChanged;
    brep::feat::FeatureId m_currentFeature{};
    bool m_updatingUi{false};
    bool m_inFieldEdit{false};
    PropertySheet m_sheet;

    QLineEdit* m_nameEdit{nullptr};
    QLineEdit* m_typeEdit{nullptr};
    QLineEdit* m_guidEdit{nullptr};
    QLabel* m_emptyLabel{nullptr};
    QLabel* m_hintLabel{nullptr};
    QWidget* m_formHost{nullptr};
    QGroupBox* m_identityGroup{nullptr};
    QGroupBox* m_paramsGroup{nullptr};
    QFormLayout* m_paramsForm{nullptr};
    QLabel* m_nameRowLabel{nullptr};
    QLabel* m_typeRowLabel{nullptr};
    std::vector<FieldRow> m_fieldRows;
};

}  // namespace brep::viewer
