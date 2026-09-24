#pragma once

#include "ui/ViewerTheme.h"

#include <QDialog>

class QCheckBox;
class QComboBox;

namespace brep::viewer
{

class ThemeSettingsDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit ThemeSettingsDialog(const ViewerTheme& theme,
                                 QWidget* parent = nullptr);

    [[nodiscard]] ViewerTheme Theme() const;

private:
    QComboBox* m_mode{nullptr};
    QCheckBox* m_invert{nullptr};
};

}  // namespace brep::viewer
