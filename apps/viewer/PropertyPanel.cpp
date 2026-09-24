#include "PropertyPanel.h"

#include <QAbstractSpinBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

namespace brep::viewer
{
namespace
{

[[nodiscard]] QString TrSource(const std::string& source)
{
    return PropertyPanel::tr(source.c_str());
}

constexpr double kSliderScale = 100.0;

// Keep lupdate source strings aligned with PropertySheet English labels.
[[maybe_unused]] const char* const kPropertySheetSources[] = {
    QT_TR_NOOP("Dimensions (parameters)"),
    QT_TR_NOOP("Length (X)"),
    QT_TR_NOOP("Height (Y)"),
    QT_TR_NOOP("Width (Z)"),
    QT_TR_NOOP("Radius"),
    QT_TR_NOOP("Weights"),
    QT_TR_NOOP("w0"),
    QT_TR_NOOP("w1"),
    QT_TR_NOOP("w2"),
    QT_TR_NOOP("w3"),
    QT_TR_NOOP("Parameter-driven · edits regenerate the model"),
    QT_TR_NOOP("No editable parameters"),
    QT_TR_NOOP("Operation: Union (Fuse)"),
    QT_TR_NOOP("Operation: Subtract (Cut)"),
    QT_TR_NOOP("Operation: Intersect (Common)"),
};

[[nodiscard]] int SliderFromValue(double value)
{
    return static_cast<int>(std::lround(value * kSliderScale));
}

[[nodiscard]] double ValueFromSlider(int slider)
{
    return static_cast<double>(slider) / kSliderScale;
}

[[nodiscard]] bool SameFieldSchema(const PropertySheet& a, const PropertySheet& b)
{
    if (a.Editable != b.Editable || a.Fields.size() != b.Fields.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < a.Fields.size(); ++i)
    {
        if (a.Fields[i].Id != b.Fields[i].Id ||
            a.Fields[i].Widget != b.Fields[i].Widget)
        {
            return false;
        }
    }
    return true;
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
    auto* formLayout = new QVBoxLayout(m_formHost);
    formLayout->setContentsMargins(0, 0, 0, 0);

    m_identityGroup = new QGroupBox(tr("Object"), m_formHost);
    auto* idForm = new QFormLayout(m_identityGroup);
    m_nameEdit = new QLineEdit(m_identityGroup);
    m_nameEdit->setReadOnly(true);
    m_typeEdit = new QLineEdit(m_identityGroup);
    m_typeEdit->setReadOnly(true);
    m_guidEdit = new QLineEdit(m_identityGroup);
    m_guidEdit->setReadOnly(true);
    m_nameRowLabel = new QLabel(tr("Name"), m_identityGroup);
    m_typeRowLabel = new QLabel(tr("Type"), m_identityGroup);
    idForm->addRow(m_nameRowLabel, m_nameEdit);
    idForm->addRow(m_typeRowLabel, m_typeEdit);
    idForm->addRow(QStringLiteral("GUID"), m_guidEdit);
    formLayout->addWidget(m_identityGroup);

    m_paramsGroup = new QGroupBox(tr("Dimensions (parameters)"), m_formHost);
    auto* paramsLayout = new QVBoxLayout(m_paramsGroup);
    m_paramsForm = new QFormLayout();
    paramsLayout->addLayout(m_paramsForm);
    m_hintLabel = new QLabel(m_paramsGroup);
    m_hintLabel->setStyleSheet(QStringLiteral("color:#888;"));
    m_hintLabel->setWordWrap(true);
    paramsLayout->addWidget(m_hintLabel);
    formLayout->addWidget(m_paramsGroup);
    formLayout->addStretch(1);

    root->addWidget(m_formHost, 1);

    clear();
}

void PropertyPanel::retranslate_ui()
{
    m_emptyLabel->setText(tr("No selection"));
    m_identityGroup->setTitle(tr("Object"));
    m_nameRowLabel->setText(tr("Name"));
    m_typeRowLabel->setText(tr("Type"));
    if (m_sheet.GroupTitle.empty())
    {
        m_paramsGroup->setTitle(tr("Dimensions (parameters)"));
    }
    else
    {
        m_paramsGroup->setTitle(TrSource(m_sheet.GroupTitle));
    }
    if (m_sheet.Hint.empty())
    {
        m_hintLabel->clear();
    }
    else
    {
        m_hintLabel->setText(TrSource(m_sheet.Hint));
    }
    const std::size_t n =
        std::min(m_fieldRows.size(), m_sheet.Fields.size());
    for (std::size_t i = 0; i < n; ++i)
    {
        if (m_fieldRows[i].Label)
        {
            m_fieldRows[i].Label->setText(TrSource(m_sheet.Fields[i].Label));
        }
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

void PropertyPanel::SetEnabled(bool enabled)
{
    m_emptyLabel->setVisible(!enabled);
    m_formHost->setVisible(enabled);
}

void PropertyPanel::ClearParamRows()
{
    while (m_paramsForm->rowCount() > 0)
    {
        m_paramsForm->removeRow(0);
    }
    m_fieldRows.clear();
}

QWidget* PropertyPanel::BuildFieldRow(const PropertyField& field)
{
    auto* spin = new QDoubleSpinBox(m_paramsGroup);
    const bool sliderSpin = field.Widget == PropertyWidget::SliderSpin;
    spin->setDecimals(sliderSpin ? 2 : 4);
    spin->setRange(field.Min, field.Max);
    spin->setSingleStep(field.Step);
    spin->setAlignment(Qt::AlignRight);
    spin->setValue(field.Value);

    const bool readOnly =
        !m_sheet.Editable || field.Widget == PropertyWidget::ReadOnly;
    if (readOnly)
    {
        spin->setReadOnly(true);
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    }

    QSlider* slider = nullptr;
    QWidget* cell = spin;
    if (sliderSpin)
    {
        auto* row = new QWidget(m_paramsGroup);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        slider = new QSlider(Qt::Horizontal, row);
        const int sliderMin = SliderFromValue(field.Min);
        const int sliderMax = SliderFromValue(field.Max);
        slider->setRange(std::max(1, sliderMin), std::max(sliderMin, sliderMax));
        slider->setValue(SliderFromValue(field.Value));
        slider->setEnabled(!readOnly);
        spin->setMaximumWidth(80);
        rowLayout->addWidget(slider, 1);
        rowLayout->addWidget(spin);
        cell = row;
    }

    FieldRow widgets;
    widgets.Id = field.Id;
    widgets.Spin = spin;
    widgets.Slider = slider;
    m_fieldRows.push_back(std::move(widgets));
    const std::size_t rowIndex = m_fieldRows.size() - 1;

    QObject::connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                     this,
                     [this, rowIndex](double)
                     {
                         if (m_updatingUi)
                         {
                             return;
                         }
                         SyncSliderFromSpin(m_fieldRows[rowIndex]);
                         OnFieldEdited(m_fieldRows[rowIndex].Id);
                     });
    if (slider)
    {
        QObject::connect(slider, &QSlider::valueChanged, this,
                         [this, rowIndex](int)
                         {
                             if (m_updatingUi)
                             {
                                 return;
                             }
                             SyncSpinFromSlider(m_fieldRows[rowIndex]);
                             OnFieldEdited(m_fieldRows[rowIndex].Id);
                         });
    }
    return cell;
}

void PropertyPanel::SyncSliderFromSpin(FieldRow& row)
{
    if (!row.Slider || !row.Spin)
    {
        return;
    }
    row.Slider->blockSignals(true);
    row.Slider->setValue(SliderFromValue(row.Spin->value()));
    row.Slider->blockSignals(false);
}

void PropertyPanel::SyncSpinFromSlider(FieldRow& row)
{
    if (!row.Slider || !row.Spin)
    {
        return;
    }
    row.Spin->blockSignals(true);
    row.Spin->setValue(ValueFromSlider(row.Slider->value()));
    row.Spin->blockSignals(false);
}

void PropertyPanel::UpdateParamRowValues(const PropertySheet& sheet)
{
    m_sheet = sheet;
    if (m_sheet.GroupTitle.empty())
    {
        m_paramsGroup->setTitle(tr("Dimensions (parameters)"));
    }
    else
    {
        m_paramsGroup->setTitle(TrSource(m_sheet.GroupTitle));
    }
    if (m_sheet.Hint.empty())
    {
        m_hintLabel->clear();
    }
    else
    {
        m_hintLabel->setText(TrSource(m_sheet.Hint));
    }

    const std::size_t n =
        std::min(m_fieldRows.size(), m_sheet.Fields.size());
    for (std::size_t i = 0; i < n; ++i)
    {
        const PropertyField& field = m_sheet.Fields[i];
        FieldRow& row = m_fieldRows[i];
        if (row.Label)
        {
            row.Label->setText(TrSource(field.Label));
        }
        if (row.Spin)
        {
            row.Spin->blockSignals(true);
            row.Spin->setRange(field.Min, field.Max);
            row.Spin->setSingleStep(field.Step);
            row.Spin->setValue(field.Value);
            row.Spin->blockSignals(false);
        }
        if (row.Slider)
        {
            row.Slider->blockSignals(true);
            const int sliderMin = SliderFromValue(field.Min);
            const int sliderMax = SliderFromValue(field.Max);
            row.Slider->setRange(std::max(1, sliderMin),
                                 std::max(sliderMin, sliderMax));
            row.Slider->setValue(SliderFromValue(field.Value));
            row.Slider->blockSignals(false);
        }
    }
}

void PropertyPanel::RebuildParamRows(const PropertySheet& sheet)
{
    if (!m_fieldRows.empty() && SameFieldSchema(m_sheet, sheet))
    {
        m_updatingUi = true;
        UpdateParamRowValues(sheet);
        m_updatingUi = false;
        return;
    }
    if (m_inFieldEdit)
    {
        QTimer::singleShot(0, this,
                           [this, sheet]() { RebuildParamRows(sheet); });
        return;
    }
    m_updatingUi = true;
    m_sheet = sheet;
    ClearParamRows();
    if (m_sheet.GroupTitle.empty())
    {
        m_paramsGroup->setTitle(tr("Dimensions (parameters)"));
    }
    else
    {
        m_paramsGroup->setTitle(TrSource(m_sheet.GroupTitle));
    }
    for (const PropertyField& field : m_sheet.Fields)
    {
        auto* label = new QLabel(TrSource(field.Label), m_paramsGroup);
        QWidget* cell = BuildFieldRow(field);
        m_fieldRows.back().Label = label;
        m_paramsForm->addRow(label, cell);
    }
    if (m_sheet.Hint.empty())
    {
        m_hintLabel->clear();
    }
    else
    {
        m_hintLabel->setText(TrSource(m_sheet.Hint));
    }
    m_updatingUi = false;
}

void PropertyPanel::clear()
{
    if (m_inFieldEdit)
    {
        QTimer::singleShot(0, this, [this]() { clear(); });
        return;
    }
    SetEnabled(false);
    m_currentFeature = {};
    m_sheet = {};
    m_nameEdit->clear();
    m_typeEdit->clear();
    m_guidEdit->clear();
    m_updatingUi = true;
    ClearParamRows();
    m_updatingUi = false;
    m_hintLabel->clear();
    m_paramsGroup->setTitle(tr("Dimensions (parameters)"));
}

void PropertyPanel::ShowEntity(entt::registry& registry, entt::entity entity)
{
    if (entity == entt::null || !registry.valid(entity))
    {
        clear();
        return;
    }

    SetEnabled(true);

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
            m_currentFeature = feat::FeatureId{fref->FeatureGuid};
            obj = m_adapter->ObjectForFeature(m_currentFeature);
        }
        else if (const auto* body = registry.try_get<ecs::BodyRef>(entity))
        {
            obj = m_adapter->ObjectForBody(body->guid);
            if (obj)
            {
                m_currentFeature = feat::FeatureId{obj->FeatureGuid};
            }
        }
    }

    if (obj)
    {
        m_typeEdit->setText(QString::fromStdString(obj->TypeName));
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

    std::optional<PrimitiveSpec> spec;
    if (m_adapter && m_currentFeature.IsValid())
    {
        spec = m_adapter->SpecFor(m_currentFeature.Guid, {});
    }

    PropertySheet sheet;
    if (spec.has_value())
    {
        sheet = Describe(*spec);
    }
    else if (obj && obj->TypeName == "Boolean" && m_adapter)
    {
        if (auto info = m_adapter->BooleanParamsFor(m_currentFeature))
        {
            sheet = DescribeBoolean(info->Op);
        }
        else
        {
            sheet.Hint = "No editable parameters";
            sheet.Editable = false;
        }
    }
    else
    {
        sheet.Hint = "No editable parameters";
        sheet.Editable = false;
    }
    RebuildParamRows(sheet);
}

void PropertyPanel::OnFieldEdited(std::string_view fieldId)
{
    if (m_updatingUi || !m_adapter || !m_currentFeature.IsValid())
    {
        return;
    }

    auto spec = m_adapter->SpecFor(m_currentFeature.Guid, {});
    if (!spec.has_value())
    {
        return;
    }

    double value = 0.0;
    bool found = false;
    for (const FieldRow& row : m_fieldRows)
    {
        if (row.Id == fieldId && row.Spin)
        {
            value = row.Spin->value();
            found = true;
            break;
        }
    }
    if (!found || !Apply(*spec, fieldId, value))
    {
        return;
    }

    m_updatingUi = true;
    const bool ok = m_adapter->SetPrimitive(m_currentFeature, *spec);
    m_updatingUi = false;
    if (!ok)
    {
        return;
    }
    m_inFieldEdit = true;
    if (m_onParamsChanged)
    {
        m_onParamsChanged(m_currentFeature);
    }
    m_inFieldEdit = false;
}

}  // namespace brep::viewer
