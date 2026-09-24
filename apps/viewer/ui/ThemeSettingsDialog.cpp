#include "ui/ThemeSettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>

namespace brep::viewer
{

ThemeSettingsDialog::ThemeSettingsDialog(const ViewerTheme& theme,
                                         QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Theme Settings"));
    setModal(true);

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;

    m_mode = new QComboBox(this);
    m_mode->addItem(tr("Dark"), static_cast<int>(ThemeMode::Dark));
    m_mode->addItem(tr("Light"), static_cast<int>(ThemeMode::Light));
    m_mode->setCurrentIndex(theme.Mode == ThemeMode::Light ? 1 : 0);
    form->addRow(tr("Theme:"), m_mode);

    m_invert = new QCheckBox(tr("Invert colors"), this);
    m_invert->setChecked(theme.Invert);
    form->addRow(QString(), m_invert);

    layout->addLayout(form);
    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                             this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

ViewerTheme ThemeSettingsDialog::Theme() const
{
    ViewerTheme theme;
    theme.Mode = static_cast<ThemeMode>(
        m_mode->currentData().toInt());
    theme.Invert = m_invert->isChecked();
    return theme;
}

}  // namespace brep::viewer
