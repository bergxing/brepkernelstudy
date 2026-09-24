#pragma once

#include <QString>

class SARibbonActionsManager;
class SARibbonBar;
class SARibbonMainWindow;

namespace brep::viewer
{

class ActionCatalog;

/// Build default Ribbon pages/panels and wire ActionsManager + XML layout.
class RibbonSetup final
{
public:
    static constexpr const char* kLayoutFileName = "ribbon_layout_v3.xml";

    /// Create categories/panels from catalog; mark customizable; register mgr.
    static void BuildDefault(SARibbonBar* bar, ActionCatalog& catalog,
                             SARibbonActionsManager* mgr);

    /// Apply saved XML if present (call after BuildDefault).
    static bool LoadLayout(SARibbonBar* bar, SARibbonActionsManager* mgr,
                           const QString& path);

    [[nodiscard]] static QString LayoutPath();

    /// Save / undo / redo on the title-bar quick access bar (reapply after XML load).
    static void SetupQuickAccessBar(SARibbonBar* bar, ActionCatalog& catalog);

    /// Minimum-mode toggle and right-side toolbar (SARibbon example chrome).
    static void SetupRibbonChrome(SARibbonBar* bar, ActionCatalog& catalog);

    static void ApplyTheme(SARibbonMainWindow* window, ActionCatalog* catalog,
                           bool lightMode);

    static void RetranslateCategories(SARibbonBar* bar,
                                      SARibbonActionsManager* mgr = nullptr);
};

}  // namespace brep::viewer
