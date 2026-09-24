#include "ui/RibbonSetup.h"

#include "assets/AssetCatalog.h"
#include "ui/ActionCatalog.h"

#include "SARibbonActionsManager.h"
#include "SARibbonBar.h"
#include "SARibbonButtonGroupWidget.h"
#include "SARibbonCategory.h"
#include "SARibbonCustomizeWidget.h"
#include "SARibbonMainWindow.h"
#include "SARibbonPanel.h"
#include "SARibbonQuickAccessBar.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QStandardPaths>
#include <QStyle>

namespace brep::viewer
{
namespace
{

QString TrRibbon(const char* source)
{
    return QCoreApplication::translate("RibbonSetup", source);
}

void MarkCustomizable(QObject* obj)
{
    if (obj)
    {
        obj->setProperty(SA_RIBBON_BAR_PROP_CAN_CUSTOMIZE, true);
    }
}

void AddLarge(SARibbonPanel* panel, ActionCatalog& catalog, const char* id)
{
    if (auto* act = catalog.Action(QString::fromUtf8(id)))
    {
        panel->addLargeAction(act);
    }
}

void AddSmall(SARibbonPanel* panel, ActionCatalog& catalog, const char* id)
{
    if (auto* act = catalog.Action(QString::fromUtf8(id)))
    {
        panel->addSmallAction(act);
    }
}

enum ActionTag : int
{
    TagFile = SARibbonActionsManager::UserDefineActionTag + 1,
    TagEdit,
    TagModeling,
    TagBoolean,
    TagTools,
    TagView,
};

void Register(SARibbonActionsManager* mgr, ActionCatalog& catalog,
              const char* id, int tag)
{
    if (auto* act = catalog.Action(QString::fromUtf8(id)))
    {
        mgr->registeAction(act, tag, QString::fromUtf8(id));
    }
}

}  // namespace

void RibbonSetup::BuildDefault(SARibbonBar* bar, ActionCatalog& catalog,
                               SARibbonActionsManager* mgr)
{
    if (!bar || !mgr)
    {
        return;
    }

    SARibbonCategory* mainCat = bar->addCategoryPage(TrRibbon("Main"));
    mainCat->setObjectName(QStringLiteral("ribbon_cat_main"));
    MarkCustomizable(mainCat);

    SARibbonPanel* savePanel = mainCat->addPanel(TrRibbon("Save"));
    savePanel->setObjectName(QStringLiteral("ribbon_panel_save"));
    MarkCustomizable(savePanel);
    AddLarge(savePanel, catalog, "file.save");

    SARibbonPanel* filePanel = mainCat->addPanel(TrRibbon("File"));
    filePanel->setObjectName(QStringLiteral("ribbon_panel_file"));
    MarkCustomizable(filePanel);
    AddLarge(filePanel, catalog, "doc.new");
    AddLarge(filePanel, catalog, "file.open");
    AddSmall(filePanel, catalog, "file.export_dxf");

    SARibbonPanel* editPanel = mainCat->addPanel(TrRibbon("Edit"));
    editPanel->setObjectName(QStringLiteral("ribbon_panel_edit"));
    MarkCustomizable(editPanel);
    AddSmall(editPanel, catalog, "edit.copy");
    AddSmall(editPanel, catalog, "edit.move");
    AddSmall(editPanel, catalog, "edit.delete");

    SARibbonCategory* model = bar->addCategoryPage(TrRibbon("Modeling"));
    model->setObjectName(QStringLiteral("ribbon_cat_model"));
    MarkCustomizable(model);
    SARibbonPanel* createPanel = model->addPanel(TrRibbon("Create"));
    createPanel->setObjectName(QStringLiteral("ribbon_panel_create"));
    MarkCustomizable(createPanel);
    AddLarge(createPanel, catalog, "part.create_box");
    AddLarge(createPanel, catalog, "part.extrude_pad");
    AddLarge(createPanel, catalog, "part.create_sphere");
    AddLarge(createPanel, catalog, "part.create_bezier");
    AddLarge(createPanel, catalog, "part.create_nurbs_curve");
    AddSmall(createPanel, catalog, "part.create_box_instant");

    SARibbonPanel* boolPanel = model->addPanel(TrRibbon("Boolean"));
    boolPanel->setObjectName(QStringLiteral("ribbon_panel_boolean"));
    MarkCustomizable(boolPanel);
    AddLarge(boolPanel, catalog, "boolean.union");
    AddLarge(boolPanel, catalog, "boolean.subtract");
    AddLarge(boolPanel, catalog, "boolean.intersect");

    SARibbonCategory* tools = bar->addCategoryPage(TrRibbon("Tools"));
    tools->setObjectName(QStringLiteral("ribbon_cat_tools"));
    MarkCustomizable(tools);
    SARibbonPanel* settingsPanel = tools->addPanel(TrRibbon("Settings"));
    settingsPanel->setObjectName(QStringLiteral("ribbon_panel_settings"));
    MarkCustomizable(settingsPanel);
    AddLarge(settingsPanel, catalog, "tools.palette");
    AddLarge(settingsPanel, catalog, "tools.snap_settings");
    AddLarge(settingsPanel, catalog, "tools.theme");
    AddLarge(settingsPanel, catalog, "tools.snap_toggle");
    AddSmall(settingsPanel, catalog, "tools.customize_ribbon");
    AddSmall(settingsPanel, catalog, "tools.reset_ribbon");

    SARibbonCategory* view = bar->addCategoryPage(TrRibbon("View"));
    view->setObjectName(QStringLiteral("ribbon_cat_view"));
    MarkCustomizable(view);
    SARibbonPanel* orientPanel = view->addPanel(TrRibbon("Orientation"));
    orientPanel->setObjectName(QStringLiteral("ribbon_panel_orient"));
    MarkCustomizable(orientPanel);
    AddLarge(orientPanel, catalog, "view.front");
    AddLarge(orientPanel, catalog, "view.back");
    AddLarge(orientPanel, catalog, "view.left");
    AddLarge(orientPanel, catalog, "view.right");
    AddLarge(orientPanel, catalog, "view.top");
    AddLarge(orientPanel, catalog, "view.bottom");
    AddLarge(orientPanel, catalog, "view.iso");
    AddLarge(orientPanel, catalog, "view.ortho");

    mgr->setTagName(TagFile, TrRibbon("File"));
    mgr->setTagName(TagEdit, TrRibbon("Edit"));
    mgr->setTagName(TagModeling, TrRibbon("Modeling"));
    mgr->setTagName(TagBoolean, TrRibbon("Boolean"));
    mgr->setTagName(TagTools, TrRibbon("Tools"));
    mgr->setTagName(TagView, TrRibbon("View"));

    Register(mgr, catalog, "doc.new", TagFile);
    Register(mgr, catalog, "file.open", TagFile);
    Register(mgr, catalog, "file.save", TagFile);
    Register(mgr, catalog, "file.export_dxf", TagFile);
    Register(mgr, catalog, "edit.undo", TagEdit);
    Register(mgr, catalog, "edit.redo", TagEdit);
    Register(mgr, catalog, "edit.copy", TagEdit);
    Register(mgr, catalog, "edit.move", TagEdit);
    Register(mgr, catalog, "edit.delete", TagEdit);
    Register(mgr, catalog, "part.create_box", TagModeling);
    Register(mgr, catalog, "part.extrude_pad", TagModeling);
    Register(mgr, catalog, "part.create_sphere", TagModeling);
    Register(mgr, catalog, "part.create_bezier", TagModeling);
    Register(mgr, catalog, "part.create_nurbs_curve", TagModeling);
    Register(mgr, catalog, "part.create_box_instant", TagModeling);
    Register(mgr, catalog, "boolean.union", TagBoolean);
    Register(mgr, catalog, "boolean.subtract", TagBoolean);
    Register(mgr, catalog, "boolean.intersect", TagBoolean);
    Register(mgr, catalog, "tools.palette", TagTools);
    Register(mgr, catalog, "tools.snap_settings", TagTools);
    Register(mgr, catalog, "tools.theme", TagTools);
    Register(mgr, catalog, "tools.snap_toggle", TagTools);
    Register(mgr, catalog, "tools.customize_ribbon", TagTools);
    Register(mgr, catalog, "tools.reset_ribbon", TagTools);
    Register(mgr, catalog, "view.front", TagView);
    Register(mgr, catalog, "view.back", TagView);
    Register(mgr, catalog, "view.left", TagView);
    Register(mgr, catalog, "view.right", TagView);
    Register(mgr, catalog, "view.top", TagView);
    Register(mgr, catalog, "view.bottom", TagView);
    Register(mgr, catalog, "view.iso", TagView);
    Register(mgr, catalog, "view.ortho", TagView);
}

void RibbonSetup::SetupQuickAccessBar(SARibbonBar* bar, ActionCatalog& catalog)
{
    if (!bar)
    {
        return;
    }
    SARibbonQuickAccessBar* qat = bar->quickAccessBar();
    if (!qat)
    {
        return;
    }
    qat->clear();
    qat->setIconSize(QSize(20, 20));
    qat->setToolButtonStyle(Qt::ToolButtonIconOnly);
    if (auto* save = catalog.Action(QStringLiteral("file.save")))
    {
        qat->addAction(save);
    }
    qat->addSeparator();
    if (auto* undo = catalog.Undo())
    {
        qat->addAction(undo);
    }
    if (auto* redo = catalog.Redo())
    {
        qat->addAction(redo);
    }
    qat->show();
    // Native frame defaults to Compact (QAT on the right). Loose puts QAT
    // on the left, matching Office / SARibbon examples.
    bar->setRibbonStyle(SARibbonBar::RibbonStyleLooseThreeRow);
}

void RibbonSetup::SetupRibbonChrome(SARibbonBar* bar, ActionCatalog& catalog)
{
    if (!bar)
    {
        return;
    }
    bar->showMinimumModeButton(true);
    SARibbonButtonGroupWidget* rightBar = bar->rightButtonGroup();
    if (!rightBar)
    {
        return;
    }
    rightBar->clear();
    rightBar->setIconSize(QSize(20, 20));
    rightBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    if (auto* customize = catalog.CustomizeRibbon())
    {
        rightBar->addAction(customize);
    }
}

QString RibbonSetup::LayoutPath()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QLatin1Char('/') + QLatin1String(kLayoutFileName);
}

bool RibbonSetup::LoadLayout(SARibbonBar* bar, SARibbonActionsManager* mgr,
                             const QString& path)
{
    if (!bar || !mgr || path.isEmpty() || !QFile::exists(path))
    {
        return false;
    }
    return sa_apply_customize_from_xml_file(path, bar, mgr);
}

void RibbonSetup::ApplyTheme(SARibbonMainWindow* window, ActionCatalog* catalog,
                             bool lightMode)
{
    if (!window)
    {
        return;
    }
    window->setRibbonTheme(lightMode ? SARibbonTheme::RibbonThemeOffice2013
                                     : SARibbonTheme::RibbonThemeDark);
    if (!catalog)
    {
        return;
    }
    QStyle* style = QApplication::style();
    const auto iconOrStd = [style](const QString& assetPath,
                                 QStyle::StandardPixmap stdPix) {
        QIcon icon(AssetCatalog::url(assetPath));
        if (icon.isNull() && style)
        {
            icon = style->standardIcon(stdPix);
        }
        return icon;
    };
    const QString undoPath =
        lightMode ? QStringLiteral("actions/undo.svg")
                  : QStringLiteral("actions/undo_dark.svg");
    const QString redoPath =
        lightMode ? QStringLiteral("actions/redo.svg")
                  : QStringLiteral("actions/redo_dark.svg");
    const QString savePath =
        lightMode ? QStringLiteral("actions/save.svg")
                  : QStringLiteral("actions/save_dark.svg");
    if (auto* save = catalog->Action(QStringLiteral("file.save")))
    {
        save->setIcon(iconOrStd(savePath, QStyle::SP_DialogSaveButton));
    }
    if (auto* undo = catalog->Undo())
    {
        undo->setIcon(iconOrStd(undoPath, QStyle::SP_ArrowBack));
    }
    if (auto* redo = catalog->Redo())
    {
        redo->setIcon(iconOrStd(redoPath, QStyle::SP_ArrowForward));
    }
    SetupQuickAccessBar(window->ribbonBar(), *catalog);
    SetupRibbonChrome(window->ribbonBar(), *catalog);
}

void RibbonSetup::RetranslateCategories(SARibbonBar* bar,
                                        SARibbonActionsManager* mgr)
{
    if (!bar)
    {
        return;
    }
    auto setCat = [](QObject* obj, const QString& title) {
        if (auto* c = qobject_cast<SARibbonCategory*>(obj))
        {
            c->setCategoryName(title);
        }
    };
    auto setPanel = [](QObject* obj, const QString& title) {
        if (auto* p = qobject_cast<SARibbonPanel*>(obj))
        {
            p->setPanelName(title);
        }
    };
    if (auto* o = bar->findChild<SARibbonCategory*>(
            QStringLiteral("ribbon_cat_main")))
    {
        setCat(o, TrRibbon("Main"));
    }
    if (auto* o = bar->findChild<SARibbonCategory*>(
            QStringLiteral("ribbon_cat_model")))
    {
        setCat(o, TrRibbon("Modeling"));
    }
    if (auto* o = bar->findChild<SARibbonCategory*>(
            QStringLiteral("ribbon_cat_tools")))
    {
        setCat(o, TrRibbon("Tools"));
    }
    if (auto* o = bar->findChild<SARibbonCategory*>(
            QStringLiteral("ribbon_cat_view")))
    {
        setCat(o, TrRibbon("View"));
    }
    setPanel(bar->findChild<SARibbonPanel*>(QStringLiteral("ribbon_panel_save")),
             TrRibbon("Save"));
    setPanel(bar->findChild<SARibbonPanel*>(QStringLiteral("ribbon_panel_file")),
             TrRibbon("File"));
    setPanel(bar->findChild<SARibbonPanel*>(QStringLiteral("ribbon_panel_edit")),
             TrRibbon("Edit"));
    setPanel(
        bar->findChild<SARibbonPanel*>(QStringLiteral("ribbon_panel_create")),
        TrRibbon("Create"));
    setPanel(
        bar->findChild<SARibbonPanel*>(QStringLiteral("ribbon_panel_boolean")),
        TrRibbon("Boolean"));
    setPanel(
        bar->findChild<SARibbonPanel*>(QStringLiteral("ribbon_panel_settings")),
        TrRibbon("Settings"));
    setPanel(
        bar->findChild<SARibbonPanel*>(QStringLiteral("ribbon_panel_orient")),
        TrRibbon("Orientation"));

    if (mgr)
    {
        mgr->setTagName(TagFile, TrRibbon("File"));
        mgr->setTagName(TagEdit, TrRibbon("Edit"));
        mgr->setTagName(TagModeling, TrRibbon("Modeling"));
        mgr->setTagName(TagBoolean, TrRibbon("Boolean"));
        mgr->setTagName(TagTools, TrRibbon("Tools"));
        mgr->setTagName(TagView, TrRibbon("View"));
    }
}

}  // namespace brep::viewer
