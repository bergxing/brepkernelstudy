#include "ui/ActionCatalog.h"

#include "assets/AssetCatalog.h"

#include <QAction>
#include <QCoreApplication>
#include <QIcon>
#include <QKeySequence>
#include <QWidget>

namespace brep::viewer
{

ActionCatalog::ActionCatalog(QWidget* parent) : QObject(parent)
{
    auto* newAct = MakeCommand(QStringLiteral("doc.new"), tr("&New"),
                               QKeySequence::New);
    newAct->setObjectName(QStringLiteral("act_file_new"));

    auto* openAct = MakeCommand(QStringLiteral("file.open"), tr("&Open…"),
                                QKeySequence::Open);
    openAct->setObjectName(QStringLiteral("act_file_open"));

    auto* saveAct = MakeCommand(QStringLiteral("file.save"), tr("&Save"),
                                QKeySequence::Save);
    saveAct->setObjectName(QStringLiteral("act_file_save"));

    auto* exportAct =
        MakeCommand(QStringLiteral("file.export_dxf"), tr("&Export DWG/DXF…"),
                    QKeySequence(QStringLiteral("Ctrl+E")));
    exportAct->setObjectName(QStringLiteral("act_file_export"));

    auto* undo = MakeCommand(QStringLiteral("edit.undo"), tr("&Undo"),
                             QKeySequence::Undo);
    undo->setObjectName(QStringLiteral("act_edit_undo"));
    undo->setIcon(QIcon(AssetCatalog::url(QStringLiteral("actions/undo.svg"))));

    auto* redo = MakeCommand(QStringLiteral("edit.redo"), tr("&Redo"),
                             QKeySequence::Redo);
    redo->setObjectName(QStringLiteral("act_edit_redo"));
    redo->setIcon(QIcon(AssetCatalog::url(QStringLiteral("actions/redo.svg"))));

    auto* copy = MakeCommand(
        QStringLiteral("edit.copy"), tr("&Copy…"),
        QKeySequence(QStringLiteral("Ctrl+Shift+C")),
        tr("Copy selected objects: base point → place point"));
    copy->setObjectName(QStringLiteral("act_edit_copy"));

    auto* move = MakeCommand(
        QStringLiteral("edit.move"), tr("&Move…"),
        QKeySequence(QStringLiteral("Ctrl+Shift+M")),
        tr("Move selected objects: base point → place point"));
    move->setObjectName(QStringLiteral("act_edit_move"));

    auto* del = MakeCommand(QStringLiteral("edit.delete"), tr("&Delete"),
                            QKeySequence::Delete, tr("Delete selected objects"));
    del->setObjectName(QStringLiteral("act_edit_delete"));

    auto* box = MakeCommand(
        QStringLiteral("part.create_box"), tr("Create &Box…"),
        QKeySequence(QStringLiteral("Ctrl+B")),
        tr("Three-point box: base corners + height"));
    box->setObjectName(QStringLiteral("act_model_box"));

    auto* sphere = MakeCommand(
        QStringLiteral("part.create_sphere"), tr("Create &Sphere…"),
        QKeySequence(QStringLiteral("Ctrl+Shift+S")),
        tr("Two-point sphere: center on surface/ground + radius"));
    sphere->setObjectName(QStringLiteral("act_model_sphere"));

    auto* bezier = MakeCommand(
        QStringLiteral("part.create_bezier"), tr("Create Be&zier Curve…"), {},
        tr("Four control points: cubic Bézier (ESC cancel)"));
    bezier->setObjectName(QStringLiteral("act_model_bezier"));

    auto* nurbs = MakeCommand(
        QStringLiteral("part.create_nurbs_curve"),
        tr("Create &NURBS Curve…"), {},
        tr("Cubic rational B-spline: pick control points, Enter to finish "
           "(ESC cancel)"));
    nurbs->setObjectName(QStringLiteral("act_model_nurbs_curve"));

    auto* extrude = MakeCommand(
        QStringLiteral("part.extrude_pad"), tr("Extrude (&Pad)…"), {},
        tr("Closed profile on ground + depth (S = symmetric, ESC cancel)"));
    extrude->setObjectName(QStringLiteral("act_model_extrude_pad"));

    auto* boxFast =
        MakeCommand(QStringLiteral("part.create_box_instant"),
                    tr("Quick Box (default size)"),
                    QKeySequence(QStringLiteral("Ctrl+Shift+B")));
    boxFast->setObjectName(QStringLiteral("act_model_box_fast"));

    const QString boolTwoBodyTip = tr(
        "First pick is the target, second is the tool (Ctrl+click). "
        "Enter or right-click Confirm. Two preselected bodies run immediately.");
    auto* bun =
        MakeCommand(QStringLiteral("boolean.union"),
                    tr("Boolean &Union (Fuse)"), {}, boolTwoBodyTip);
    bun->setObjectName(QStringLiteral("act_model_boolean_union"));
    auto* bsub = MakeCommand(
        QStringLiteral("boolean.subtract"), tr("Boolean &Subtract (Cut)"), {},
        boolTwoBodyTip);
    bsub->setObjectName(QStringLiteral("act_model_boolean_subtract"));
    auto* bint =
        MakeCommand(QStringLiteral("boolean.intersect"),
                    tr("Boolean &Intersect (Common)"), {}, boolTwoBodyTip);
    bint->setObjectName(QStringLiteral("act_model_boolean_intersect"));

    auto* palette = MakeUi(QStringLiteral("tools.palette"),
                           tr("Command &Palette…"));
    palette->setObjectName(QStringLiteral("act_tools_palette"));
    palette->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+P")));
    connect(palette, &QAction::triggered, this,
            &ActionCatalog::PaletteRequested);

    auto* snapSet = MakeUi(QStringLiteral("tools.snap_settings"),
                           tr("Snap &Settings…"));
    snapSet->setObjectName(QStringLiteral("act_tools_snap_settings"));
    connect(snapSet, &QAction::triggered, this,
            &ActionCatalog::SnapSettingsRequested);

    auto* theme = MakeUi(QStringLiteral("tools.theme"),
                         tr("&Theme Settings…"));
    theme->setObjectName(QStringLiteral("act_tools_theme_settings"));
    connect(theme, &QAction::triggered, this,
            &ActionCatalog::ThemeSettingsRequested);

    auto* customize = MakeUi(QStringLiteral("tools.customize_ribbon"),
                             tr("&Customize Ribbon…"));
    customize->setObjectName(QStringLiteral("act_tools_customize_ribbon"));
    connect(customize, &QAction::triggered, this,
            &ActionCatalog::CustomizeRibbonRequested);

    auto* resetRibbon = MakeUi(QStringLiteral("tools.reset_ribbon"),
                               tr("&Reset Ribbon Layout"));
    resetRibbon->setObjectName(QStringLiteral("act_tools_reset_ribbon"));
    connect(resetRibbon, &QAction::triggered, this,
            &ActionCatalog::ResetRibbonRequested);

    auto* snapToggle =
        MakeUi(QStringLiteral("tools.snap_toggle"), tr("Snap"), true);
    snapToggle->setObjectName(QStringLiteral("tb_snap_enabled"));
    snapToggle->setToolTip(tr("Enable AccuSnap (F3)"));
    connect(snapToggle, &QAction::toggled, this, &ActionCatalog::SnapToggled);

    auto* ortho = MakeUi(QStringLiteral("view.ortho"), tr("Ortho"), true);
    ortho->setObjectName(QStringLiteral("tb_view_ortho"));
    connect(ortho, &QAction::toggled, this, &ActionCatalog::OrthoToggled);

    struct ViewSpec
    {
        const char* id;
        const char* file;
        const char* objectName;
        char face;
        const char* label;
    };
    constexpr ViewSpec kViews[] = {
        {"view.front", "front.png", "tb_view_front", 'f', "Front"},
        {"view.back", "back.png", "tb_view_back", 'k', "Back"},
        {"view.left", "left.png", "tb_view_left", 'l', "Left"},
        {"view.right", "right.png", "tb_view_right", 'r', "Right"},
        {"view.top", "top.png", "tb_view_top", 't', "Top"},
        {"view.bottom", "bottom.png", "tb_view_bottom", 'b', "Bottom"},
        {"view.iso", "iso.png", "tb_view_iso", 'h', "Iso"},
    };
    for (const auto& spec : kViews)
    {
        auto* act = MakeUi(QString::fromUtf8(spec.id),
                           QCoreApplication::translate(
                               "brep::viewer::ActionCatalog", spec.label));
        act->setObjectName(QString::fromUtf8(spec.objectName));
        act->setIcon(AssetCatalog::icon(QStringLiteral("views/") +
                                        QString::fromUtf8(spec.file)));
        const char face = spec.face;
        connect(act, &QAction::triggered, this,
                [this, face] { emit StandardViewRequested(face); });
    }
}

QAction* ActionCatalog::MakeCommand(const QString& id, const QString& text,
                                    const QKeySequence& shortcut,
                                    const QString& tip)
{
    auto* act = new QAction(text, parent());
    act->setMenuRole(QAction::NoRole);
    act->setData(id);
    if (!shortcut.isEmpty())
    {
        act->setShortcut(shortcut);
    }
    if (!tip.isEmpty())
    {
        act->setToolTip(tip);
    }
    connect(act, &QAction::triggered, this, [this, id] {
        emit CommandTriggered(id);
    });
    m_actions.insert(id, act);
    return act;
}

QAction* ActionCatalog::MakeUi(const QString& id, const QString& text,
                               bool checkable)
{
    auto* act = new QAction(text, parent());
    act->setMenuRole(QAction::NoRole);
    act->setData(id);
    act->setCheckable(checkable);
    m_actions.insert(id, act);
    return act;
}

QAction* ActionCatalog::Action(const QString& id) const
{
    return m_actions.value(id, nullptr);
}

QList<QAction*> ActionCatalog::AllActions() const
{
    return m_actions.values();
}

void ActionCatalog::Retranslate()
{
    auto setText = [this](const char* id, const QString& text) {
        if (auto* a = Action(QString::fromUtf8(id)))
        {
            a->setText(text);
        }
    };
    auto setTip = [this](const char* id, const QString& tip) {
        if (auto* a = Action(QString::fromUtf8(id)))
        {
            a->setToolTip(tip);
        }
    };

    setText("doc.new", tr("&New"));
    setText("file.open", tr("&Open…"));
    setText("file.save", tr("&Save"));
    setText("file.export_dxf", tr("&Export DWG/DXF…"));
    setText("edit.undo", tr("&Undo"));
    setText("edit.redo", tr("&Redo"));
    setText("edit.copy", tr("&Copy…"));
    setTip("edit.copy",
           tr("Copy selected objects: base point → place point"));
    setText("edit.move", tr("&Move…"));
    setTip("edit.move",
           tr("Move selected objects: base point → place point"));
    setText("edit.delete", tr("&Delete"));
    setTip("edit.delete", tr("Delete selected objects"));
    setText("part.create_box", tr("Create &Box…"));
    setTip("part.create_box", tr("Three-point box: base corners + height"));
    setText("part.create_sphere", tr("Create &Sphere…"));
    setTip("part.create_sphere",
           tr("Two-point sphere: center on surface/ground + radius"));
    setText("part.create_bezier", tr("Create Be&zier Curve…"));
    setTip("part.create_bezier",
           tr("Four control points: cubic Bézier (ESC cancel)"));
    setText("part.create_nurbs_curve", tr("Create &NURBS Curve…"));
    setTip("part.create_nurbs_curve",
           tr("Cubic rational B-spline: pick control points, Enter to finish "
              "(ESC cancel)"));
    setText("part.extrude_pad", tr("Extrude (&Pad)…"));
    setTip("part.extrude_pad",
           tr("Closed profile on ground + depth (S = symmetric, ESC cancel)"));
    setText("part.create_box_instant", tr("Quick Box (default size)"));
    setText("boolean.union", tr("Boolean &Union (Fuse)"));
    setText("boolean.subtract", tr("Boolean &Subtract (Cut)"));
    setText("boolean.intersect", tr("Boolean &Intersect (Common)"));
    const QString boolTwoBodyTip = tr(
        "First pick is the target, second is the tool (Ctrl+click). "
        "Enter or right-click Confirm. Two preselected bodies run immediately.");
    setTip("boolean.union", boolTwoBodyTip);
    setTip("boolean.subtract", boolTwoBodyTip);
    setTip("boolean.intersect", boolTwoBodyTip);
    setText("tools.palette", tr("Command &Palette…"));
    setText("tools.snap_settings", tr("Snap &Settings…"));
    setText("tools.theme", tr("&Theme Settings…"));
    setText("tools.customize_ribbon", tr("&Customize Ribbon…"));
    setText("tools.reset_ribbon", tr("&Reset Ribbon Layout"));
    setText("tools.snap_toggle", tr("Snap"));
    setTip("tools.snap_toggle", tr("Enable AccuSnap (F3)"));
    setText("view.ortho", tr("Ortho"));
    setText("view.front", tr("Front"));
    setText("view.back", tr("Back"));
    setText("view.left", tr("Left"));
    setText("view.right", tr("Right"));
    setText("view.top", tr("Top"));
    setText("view.bottom", tr("Bottom"));
    setText("view.iso", tr("Iso"));
}

}  // namespace brep::viewer
