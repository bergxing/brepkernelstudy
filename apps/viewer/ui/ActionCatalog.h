#pragma once

#include <QHash>
#include <QKeySequence>
#include <QObject>
#include <QString>

class QAction;
class QWidget;

namespace brep::viewer
{

/// Shared QActions for menus, Ribbon, and customize dialog (one per id).
class ActionCatalog final : public QObject
{
    Q_OBJECT

public:
    explicit ActionCatalog(QWidget* parent);

    [[nodiscard]] QAction* Action(const QString& id) const;
    [[nodiscard]] QList<QAction*> AllActions() const;
    void Retranslate();

    // Convenience accessors used by MainWindow.
    [[nodiscard]] QAction* Undo() const
    {
        return Action(QStringLiteral("edit.undo"));
    }
    [[nodiscard]] QAction* Redo() const
    {
        return Action(QStringLiteral("edit.redo"));
    }
    [[nodiscard]] QAction* SnapToggle() const
    {
        return Action(QStringLiteral("tools.snap_toggle"));
    }
    [[nodiscard]] QAction* Ortho() const
    {
        return Action(QStringLiteral("view.ortho"));
    }
    [[nodiscard]] QAction* CustomizeRibbon() const
    {
        return Action(QStringLiteral("tools.customize_ribbon"));
    }
    [[nodiscard]] QAction* ResetRibbon() const
    {
        return Action(QStringLiteral("tools.reset_ribbon"));
    }

signals:
    void CommandTriggered(const QString& commandId);
    void PaletteRequested();
    void SnapSettingsRequested();
    void ThemeSettingsRequested();
    void CustomizeRibbonRequested();
    void ResetRibbonRequested();
    void SnapToggled(bool enabled);
    void OrthoToggled(bool enabled);
    void StandardViewRequested(char face);

private:
    QAction* MakeCommand(const QString& id, const QString& text,
                         const QKeySequence& shortcut = {},
                         const QString& tip = {});
    QAction* MakeUi(const QString& id, const QString& text,
                    bool checkable = false);

    QHash<QString, QAction*> m_actions;
};

}  // namespace brep::viewer
