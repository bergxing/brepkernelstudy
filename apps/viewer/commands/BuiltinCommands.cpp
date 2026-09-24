#include "commands/CommandRegistry.h"
#include "commands/DocumentHistory.h"
#include "commands/ITool.h"
#include "commands/tools/BooleanExecute.h"
#include "commands/tools/BooleanTwoBodyTool.h"
#include "commands/tools/CopyTool.h"
#include "commands/tools/CreateBezierTool.h"
#include "commands/tools/CreateBoxTool.h"
#include "commands/tools/CreateNurbsCurveTool.h"
#include "commands/tools/CreateSphereTool.h"
#include "commands/tools/ExtrudePadTool.h"
#include "commands/tools/MoveTool.h"
#include "ecs/Systems.h"

#include "adapter/ISceneServiceFactory.h"
#include "api/Core.h"
#include "api/Modeling.h"
#include "ecs/Components.h"
#include "io/DxfExport.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

#include <array>
#include <optional>
#include <variant>
#include <vector>

namespace brep::viewer::commands
{
namespace
{

QString TrCmd(const char* source)
{
  return QCoreApplication::translate("BuiltinCommands", source);
}

class NewDocumentCommand final : public ICommand
{
 public:
  [[nodiscard]] std::string_view id() const noexcept override
  {
    return "doc.new";
  }
  [[nodiscard]] std::string_view title() const noexcept override
  {
    return "New Document";
  }
  [[nodiscard]] CommandKind kind() const noexcept override
  {
    return CommandKind::Instant;
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override
  {
    return ctx.World != nullptr && ctx.Session != nullptr;
  }

  CommandResult execute(CommandContext& ctx) override
  {
    if (ctx.History) ctx.History->clear();
    ctx.Session->new_blank_document(*ctx.World);
    if (ctx.AfterDocumentReset) ctx.AfterDocumentReset();
    if (ctx.RequestRedraw) ctx.RequestRedraw();
    if (ctx.RefreshUi) ctx.RefreshUi();
    const QString msg = TrCmd("Created blank document");
    if (ctx.ReportStatus) ctx.ReportStatus(msg);
    return CommandResult::Ok(msg);
  }
};

class SaveXlCommand final : public ICommand
{
 public:
  [[nodiscard]] std::string_view id() const noexcept override
  {
    return "file.save";
  }
  [[nodiscard]] std::string_view title() const noexcept override
  {
    return "Save (.xl)";
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override
  {
    return ctx.World != nullptr && ctx.World->Document() != nullptr &&
           ctx.Session != nullptr;
  }

  CommandResult execute(CommandContext& ctx) override
  {
    QString path = ctx.Session->path();
    if (path.isEmpty() ||
        !path.endsWith(QStringLiteral(".xl"), Qt::CaseInsensitive))
    {
      QString start = path;
      if (start.isEmpty())
      {
        start = QDir::homePath() + QStringLiteral("/untitled.xl");
      }
      else
      {
        QFileInfo fi(start);
        start = fi.path() + QLatin1Char('/') + fi.completeBaseName() +
                QStringLiteral(".xl");
      }
      path = QFileDialog::getSaveFileName(
          ctx.ParentWidget, TrCmd("Save Document"), start,
          QStringLiteral("XCAD Document (*.xl);;All Files (*)"));
      if (path.isEmpty())
      {
        return CommandResult::Cancelled(TrCmd("Save cancelled"));
      }
      if (!path.endsWith(QStringLiteral(".xl"), Qt::CaseInsensitive))
      {
        path += QStringLiteral(".xl");
      }
    }

    if (!ctx.DocumentService)
    {
      return CommandResult::Failed(TrCmd("Document service unavailable"));
    }

    auto result = ctx.DocumentService->save(*ctx.World->Document(),
                                            path.toStdString());
    if (!result.ok)
    {
      const QString err = QString::fromStdString(result.error);
      if (ctx.ParentWidget)
      {
        QMessageBox::critical(ctx.ParentWidget, TrCmd("Save failed"),
                              err);
      }
      return CommandResult::Failed(err);
    }

    ctx.World->Document()->SetPath(path.toStdString());
    ctx.World->Document()->MarkClean();
    ctx.Session->set_document_path(path);
    if (ctx.RefreshUi) ctx.RefreshUi();
    const QString msg = TrCmd("Saved: %1").arg(path);
    if (ctx.ReportStatus) ctx.ReportStatus(msg);
    return CommandResult::Ok(msg);
  }
};

class OpenXlCommand final : public ICommand
{
 public:
  [[nodiscard]] std::string_view id() const noexcept override
  {
    return "file.open";
  }
  [[nodiscard]] std::string_view title() const noexcept override
  {
    return "Open (.xl)";
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override
  {
    return ctx.World != nullptr && ctx.Session != nullptr;
  }

  CommandResult execute(CommandContext& ctx) override
  {
    QString start = ctx.Session->path();
    if (start.isEmpty()) start = QDir::homePath();
    const QString path = QFileDialog::getOpenFileName(
        ctx.ParentWidget, TrCmd("Open Document"), start,
        QStringLiteral("XCAD Document (*.xl);;All Files (*)"));
    if (path.isEmpty())
    {
      return CommandResult::Cancelled(TrCmd("Open cancelled"));
    }
    return open_xl_file(ctx, path);
  }
};

class ExportDxfCommand final : public ICommand
    {
 public:
  [[nodiscard]] std::string_view id() const noexcept override
  {
    return "file.export_dxf";
  }
  [[nodiscard]] std::string_view title() const noexcept override
  {
    return "Export DXF";
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override
  {
    return ctx.World != nullptr && ctx.Session != nullptr;
  }

  CommandResult execute(CommandContext& ctx) override
  {
    QString start = ctx.Session->path();
    if (start.isEmpty())
    {
      start = QDir::homePath() + QStringLiteral("/untitled.dxf");
    }

    QString path = QFileDialog::getSaveFileName(
        ctx.ParentWidget, TrCmd("Export DWG/DXF"), start,
        QStringLiteral("CAD Drawing (*.dxf);;All Files (*)"));
    if (path.isEmpty())
    {
      return CommandResult::Cancelled(TrCmd("Export cancelled"));
    }
    if (!path.endsWith(QStringLiteral(".dxf"), Qt::CaseInsensitive))
    {
      path += QStringLiteral(".dxf");
    }

    std::vector<io::DxfSegment> segments;
    auto view =
        ctx.World->registry().view<ecs::MeshComponent, ecs::Transform>();
    for (auto entity : view)
    {
      const auto& mesh = view.get<ecs::MeshComponent>(entity);
      const auto& xform = view.get<ecs::Transform>(entity);
      io::append_edge_segments(mesh.edges, xform.position, segments);
    }

    if (!io::write_edges_dxf(path.toStdString(), segments))
    {
      const QString err = QString::fromStdString(io::last_dxf_error());
      if (ctx.ParentWidget)
      {
        QMessageBox::critical(ctx.ParentWidget, TrCmd("Export failed"),
                              err);
      }
      return CommandResult::Failed(err);
    }

    ctx.Session->set_export_path(path);
    if (ctx.RefreshUi) ctx.RefreshUi();
    const QString msg = TrCmd("Exported DXF (%1 segments): %2")
                            .arg(segments.size())
                            .arg(path);
    if (ctx.ReportStatus) ctx.ReportStatus(msg);
    return CommandResult::Ok(msg);
  }
};

class CopyCommand final : public ICommand
    {
 public:
  [[nodiscard]] std::string_view id() const noexcept override
  {
    return "edit.copy";
  }
  [[nodiscard]] std::string_view title() const noexcept override
  {
    return "Copy";
  }
  [[nodiscard]] CommandKind kind() const noexcept override
  {
    return CommandKind::Interactive;
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override
  {
    return ctx.World != nullptr && ctx.World->Document() != nullptr &&
           ctx.World->Document()->MainPart() != nullptr;
  }

  [[nodiscard]] std::unique_ptr<ITool> make_tool(
      CommandContext& /*ctx*/) const override
  {
    return std::make_unique<CopyTool>();
  }
};

class MoveCommand final : public ICommand
{
 public:
  [[nodiscard]] std::string_view id() const noexcept override
  {
    return "edit.move";
  }
  [[nodiscard]] std::string_view title() const noexcept override
  {
    return "Move";
  }
  [[nodiscard]] CommandKind kind() const noexcept override
  {
    return CommandKind::Interactive;
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override
  {
    return ctx.World != nullptr && ctx.World->Document() != nullptr &&
           ctx.World->Document()->MainPart() != nullptr;
  }

  [[nodiscard]] std::unique_ptr<ITool> make_tool(
      CommandContext& /*ctx*/) const override
  {
    return std::make_unique<MoveTool>();
  }
};

/// Interactive two-click box (Phase 3 tool).
class CreateBoxCommand final : public ICommand
{
 public:
  [[nodiscard]] std::string_view id() const noexcept override
  {
    return "part.create_box";
  }
  [[nodiscard]] std::string_view title() const noexcept override
  {
    return "Create Box (interactive)";
  }
  [[nodiscard]] CommandKind kind() const noexcept override
  {
    return CommandKind::Interactive;
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override
  {
    return ctx.World != nullptr && ctx.World->Document() != nullptr &&
           ctx.World->Document()->MainPart() != nullptr;
  }

  [[nodiscard]] std::unique_ptr<ITool> make_tool(
      CommandContext& /*ctx*/) const override
  {
    return std::make_unique<CreateBoxTool>();
  }
};

class CreateSphereCommand final : public ICommand
{
 public:
  [[nodiscard]] std::string_view id() const noexcept override
  {
    return "part.create_sphere";
  }
  [[nodiscard]] std::string_view title() const noexcept override
  {
    return "Create Sphere (interactive)";
  }
  [[nodiscard]] CommandKind kind() const noexcept override
  {
    return CommandKind::Interactive;
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override
  {
    return ctx.World != nullptr && ctx.World->Document() != nullptr &&
           ctx.World->Document()->MainPart() != nullptr;
  }

  [[nodiscard]] std::unique_ptr<ITool> make_tool(
      CommandContext& /*ctx*/) const override
  {
    return std::make_unique<CreateSphereTool>();
  }
};

class CreateBezierCommand final : public ICommand
{
 public:
  [[nodiscard]] std::string_view id() const noexcept override
  {
    return "part.create_bezier";
  }
  [[nodiscard]] std::string_view title() const noexcept override
  {
    return "Create Bezier Curve (interactive)";
  }
  [[nodiscard]] CommandKind kind() const noexcept override
  {
    return CommandKind::Interactive;
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override
  {
    return ctx.World != nullptr && ctx.World->Document() != nullptr &&
           ctx.World->Document()->MainPart() != nullptr;
  }

  [[nodiscard]] std::unique_ptr<ITool> make_tool(
      CommandContext& /*ctx*/) const override
  {
    return std::make_unique<CreateBezierTool>();
  }
};

class CreateNurbsCurveCommand final : public ICommand
{
 public:
  [[nodiscard]] std::string_view id() const noexcept override
  {
    return "part.create_nurbs_curve";
  }
  [[nodiscard]] std::string_view title() const noexcept override
  {
    return "Create NURBS Curve (interactive)";
  }
  [[nodiscard]] CommandKind kind() const noexcept override
  {
    return CommandKind::Interactive;
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override
  {
    return ctx.World != nullptr && ctx.World->Document() != nullptr &&
           ctx.World->Document()->MainPart() != nullptr;
  }

  [[nodiscard]] std::unique_ptr<ITool> make_tool(
      CommandContext& /*ctx*/) const override
  {
    return std::make_unique<CreateNurbsCurveTool>();
  }
};

class ExtrudePadCommand final : public ICommand
{
 public:
  [[nodiscard]] std::string_view id() const noexcept override
  {
    return "part.extrude_pad";
  }
  [[nodiscard]] std::string_view title() const noexcept override
  {
    return "Extrude (Pad)";
  }
  [[nodiscard]] CommandKind kind() const noexcept override
  {
    return CommandKind::Interactive;
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override
  {
    return ctx.World != nullptr && ctx.World->Document() != nullptr &&
           ctx.World->Document()->MainPart() != nullptr;
  }

  [[nodiscard]] std::unique_ptr<ITool> make_tool(
      CommandContext& /*ctx*/) const override
  {
    return std::make_unique<ExtrudePadTool>();
  }
};

class ElevateBezierCommand final : public ICommand
{
 public:
  [[nodiscard]] std::string_view id() const noexcept override
  {
    return "part.elevate_bezier";
  }
  [[nodiscard]] std::string_view title() const noexcept override
  {
    return "Elevate Bezier Degree";
  }
  [[nodiscard]] CommandKind kind() const noexcept override
  {
    return CommandKind::Instant;
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override
  {
    return ctx.World && ctx.World->Document() && ctx.Scene &&
           ecs::selected_count(ctx.World->registry()) == 1;
  }

  [[nodiscard]] CommandResult execute(CommandContext& ctx) override
  {
    const entt::entity e = ecs::selected_entity(ctx.World->registry());
    if (e == entt::null ||
        !ctx.World->registry().all_of<ecs::FeatureRef, ecs::BodyRef>(e))
    {
      return CommandResult::Failed(QCoreApplication::translate(
          "BezierEdit", "Select one bezier curve"));
    }
    const Guid featureGuid =
        ctx.World->registry().get<ecs::FeatureRef>(e).FeatureGuid;
    auto specOpt = ctx.Scene->SpecFor(featureGuid, Guid{});
    if (!specOpt || !std::holds_alternative<BezierSpec>(*specOpt))
    {
      return CommandResult::Failed(QCoreApplication::translate(
          "BezierEdit", "Selection is not a bezier"));
    }
    BezierSpec before = std::get<BezierSpec>(*specOpt);
    if (before.SegmentCount != 1)
    {
      return CommandResult::Failed(QCoreApplication::translate(
          "BezierEdit", "Elevate supports single-segment only"));
    }
    BezierSpec after = ElevateBezierDegree(before);
    if (!ctx.Scene->SetPrimitive(feat::FeatureId{featureGuid}, after))
    {
      return CommandResult::Failed(QCoreApplication::translate(
          "BezierEdit", "Failed to elevate bezier"));
    }
    Part* part = ctx.Scene->MainPart();
    if (part)
    {
      Material mat = ctx.WoodAlbedoPath.empty()
                         ? Material{}
                         : MakeWoodMaterial(ctx.WoodAlbedoPath);
      ctx.World->SyncPartBodies(*part, std::move(mat));
    }
    if (ctx.History)
    {
      const std::string wood = ctx.WoodAlbedoPath;
      ctx.History->push(DocumentHistory::Entry{
          .label = QCoreApplication::translate("BezierEdit", "Elevate bezier"),
          .undo =
              [world = ctx.World, wood, factory = ctx.SceneFactory,
               redraw = ctx.RequestRedraw]()
              {
                if (!world)
                {
                  return;
                }
                adapter::WithScene(factory, world->Document(),
                                   [](adapter::ISceneService& s)
                                   { s.UndoFeature(); });
                if (Part* p = world->Document()
                                  ? world->Document()->MainPart()
                                  : nullptr)
                {
                  Material m =
                      wood.empty() ? Material{} : MakeWoodMaterial(wood);
                  world->SyncPartBodies(*p, std::move(m));
                }
                if (redraw)
                {
                  redraw();
                }
              },
          .redo =
              [world = ctx.World, wood, factory = ctx.SceneFactory,
               redraw = ctx.RequestRedraw]()
              {
                if (!world)
                {
                  return;
                }
                adapter::WithScene(factory, world->Document(),
                                   [](adapter::ISceneService& s)
                                   { s.RedoFeature(); });
                if (Part* p = world->Document()
                                  ? world->Document()->MainPart()
                                  : nullptr)
                {
                  Material m =
                      wood.empty() ? Material{} : MakeWoodMaterial(wood);
                  world->SyncPartBodies(*p, std::move(m));
                }
                if (redraw)
                {
                  redraw();
                }
              },
      });
    }
    if (ctx.RequestRedraw)
    {
      ctx.RequestRedraw();
    }
    return CommandResult::Ok(QCoreApplication::translate(
        "BezierEdit", "Elevated bezier degree"));
  }
};

/// One-shot default-size box (keeps a quick path).
class CreateBoxInstantCommand final : public ICommand
{
 public:
  [[nodiscard]] std::string_view id() const noexcept override
  {
    return "part.create_box_instant";
  }
  [[nodiscard]] std::string_view title() const noexcept override
  {
    return "Create Box (instant)";
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override
  {
    return ctx.World != nullptr && ctx.World->Document() != nullptr &&
           ctx.World->Document()->MainPart() != nullptr;
  }

  CommandResult execute(CommandContext& ctx) override
  {
    using namespace brep;
    if (!ctx.Scene)
    {
      return CommandResult::Failed(TrCmd("Invalid context"));
    }
    adapter::ISceneService& scene = *ctx.Scene;
    Part* part = scene.MainPart();
    if (!part)
    {
      return CommandResult::Failed(TrCmd("No active Part"));
    }
    BoxSpec spec{
        .Min = Point3d{0, 0, 0},
        .Max = Point3d{2, 1, 3},
        .Name = "box",
    };
    Body* body = scene.AddPrimitive(spec);
    if (!body)
    {
      return CommandResult::Failed(TrCmd("Create box failed"));
    }

    Guid feature_guid{};
    if (auto obj = scene.ObjectForBody(body->Guid))
    {
      feature_guid = obj->FeatureGuid;
      scene.RecordAppendPrimitive(feat::FeatureId{feature_guid}, spec);
    }

    Material material = ctx.WoodAlbedoPath.empty()
                            ? Material{}
                            : MakeWoodMaterial(ctx.WoodAlbedoPath);
    auto mesh = scene.MeshForBody(body->Guid);
    ctx.World->CreateBodyRenderable(body->Name, body->Guid,
                                      std::move(mesh.Faces),
                                      std::move(mesh.Edges), material,
                                      Point3d{}, feature_guid);
    if (ctx.Session) ctx.Session->MarkDirty();
    if (ctx.RequestRedraw) ctx.RequestRedraw();

    const std::string wood = ctx.WoodAlbedoPath;
    ecs::World* world = ctx.World;
    Part* part_ptr = part;
    if (ctx.History)
    {
      ctx.History->push({
          .label = TrCmd("Create box"),
          .undo =
              [world, part_ptr, wood, factory = ctx.SceneFactory,
               session = ctx.Session, redraw = ctx.RequestRedraw,
               refresh = ctx.RefreshUi] {
                if (!world || !part_ptr) return;
                adapter::WithScene(
                    factory, world->Document(),
                    [](adapter::ISceneService& scene) { scene.UndoFeature(); });
                Material mat =
                    wood.empty() ? Material{} : MakeWoodMaterial(wood);
                world->SyncPartBodies(*part_ptr, std::move(mat));
                if (session) session->MarkDirty();
                if (redraw) redraw();
                if (refresh) refresh();
              },
          .redo =
              [world, part_ptr, wood, factory = ctx.SceneFactory,
               session = ctx.Session, redraw = ctx.RequestRedraw,
               refresh = ctx.RefreshUi] {
                if (!world || !part_ptr) return;
                adapter::WithScene(
                    factory, world->Document(),
                    [](adapter::ISceneService& scene) { scene.RedoFeature(); });
                Material mat =
                    wood.empty() ? Material{} : MakeWoodMaterial(wood);
                world->SyncPartBodies(*part_ptr, std::move(mat));
                if (session) session->MarkDirty();
                if (redraw) redraw();
                if (refresh) refresh();
              },
      });
    }

    if (ctx.RefreshUi) ctx.RefreshUi();
    const QString msg =
        TrCmd("Created Body '%1'")
            .arg(QString::fromStdString(body->Guid.ToString()));
    if (ctx.ReportStatus) ctx.ReportStatus(msg);
    return CommandResult::Ok(msg);
  }
};

class DeleteSelectionCommand final : public ICommand
    {
 public:
  [[nodiscard]] std::string_view id() const noexcept override
  {
    return "edit.delete";
  }
  [[nodiscard]] std::string_view title() const noexcept override
  {
    return "Delete";
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override
  {
    return ctx.World != nullptr && ctx.World->Document() != nullptr &&
           ctx.World->Document()->MainPart() != nullptr &&
           ecs::selected_count(ctx.World->registry()) > 0;
  }

  CommandResult execute(CommandContext& ctx) override
  {
    if (!ctx.Scene)
    {
      return CommandResult::Failed(TrCmd("Invalid context"));
    }
    adapter::ISceneService& scene = *ctx.Scene;
    Part* part = scene.MainPart();
    if (!part)
    {
      return CommandResult::Failed(TrCmd("No active Part"));
    }

    auto& registry = ctx.World->registry();
    std::vector<feat::FeatureId> to_remove;
    auto view = registry.view<ecs::SelectedTag>();
    for (auto entity : view)
    {
      Guid feature_guid{};
      Guid body_guid{};
      if (const auto* fref = registry.try_get<ecs::FeatureRef>(entity))
      {
        feature_guid = fref->FeatureGuid;
      }
      if (const auto* body = registry.try_get<ecs::BodyRef>(entity))
      {
        body_guid = body->guid;
      }
      if (auto fid = scene.FeatureIdFor(feature_guid, body_guid))
      {
        to_remove.push_back(*fid);
      }
    }

    // Stable unique (multi-select of same feature should only remove once).
    std::sort(to_remove.begin(), to_remove.end(),
              [](const feat::FeatureId& a, const feat::FeatureId& b)
    {
                return a.Guid < b.Guid;
              });
    to_remove.erase(std::unique(to_remove.begin(), to_remove.end()),
                    to_remove.end());

    if (to_remove.empty())
    {
      return CommandResult::Failed(TrCmd("Invalid context"));
    }

    int removed = 0;
    for (const auto& fid : to_remove)
    {
      if (scene.RemoveFeature(fid)) ++removed;
    }
    if (removed == 0)
    {
      return CommandResult::Failed(TrCmd("Delete failed"));
    }

    Material material = ctx.WoodAlbedoPath.empty()
                            ? Material{}
                            : MakeWoodMaterial(ctx.WoodAlbedoPath);
    ctx.World->SyncPartBodies(*part, std::move(material));
    if (ctx.Session) ctx.Session->MarkDirty();
    if (ctx.RequestRedraw) ctx.RequestRedraw();

    const std::string wood = ctx.WoodAlbedoPath;
    ecs::World* world = ctx.World;
    Part* part_ptr = part;
    const int undo_steps = removed;
    if (ctx.History)
    {
      ctx.History->push({
          .label = TrCmd("Delete %1 object(s)").arg(removed),
          .undo =
              [world, part_ptr, wood, undo_steps, factory = ctx.SceneFactory,
               session = ctx.Session, redraw = ctx.RequestRedraw,
               refresh = ctx.RefreshUi] {
                if (!world || !part_ptr) return;
                adapter::WithScene(
                    factory, world->Document(),
                    [undo_steps](adapter::ISceneService& scene) {
                      scene.UndoFeature(undo_steps);
                    });
                Material mat =
                    wood.empty() ? Material{} : MakeWoodMaterial(wood);
                world->SyncPartBodies(*part_ptr, std::move(mat));
                if (session) session->MarkDirty();
                if (redraw) redraw();
                if (refresh) refresh();
              },
          .redo =
              [world, part_ptr, wood, undo_steps, factory = ctx.SceneFactory,
               session = ctx.Session, redraw = ctx.RequestRedraw,
               refresh = ctx.RefreshUi] {
                if (!world || !part_ptr) return;
                adapter::WithScene(
                    factory, world->Document(),
                    [undo_steps](adapter::ISceneService& scene) {
                      scene.RedoFeature(undo_steps);
                    });
                Material mat =
                    wood.empty() ? Material{} : MakeWoodMaterial(wood);
                world->SyncPartBodies(*part_ptr, std::move(mat));
                if (session) session->MarkDirty();
                if (redraw) redraw();
                if (refresh) refresh();
              },
      });
    }

    if (ctx.RefreshUi) ctx.RefreshUi();
    const QString msg = TrCmd("Deleted %1 object(s)").arg(removed);
    if (ctx.ReportStatus) ctx.ReportStatus(msg);
    return CommandResult::Ok(msg);
  }
};

class UndoCommand final : public ICommand
{
 public:
  [[nodiscard]] std::string_view id() const noexcept override
  {
    return "edit.undo";
  }
  [[nodiscard]] std::string_view title() const noexcept override
  {
    return "Undo";
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override
  {
    return ctx.History && ctx.History->can_undo();
  }

  CommandResult execute(CommandContext& ctx) override
  {
    const QString label = ctx.History->undo_label();
    if (!ctx.History->undo())
    {
      return CommandResult::Failed(TrCmd("Nothing to undo"));
    }
    if (ctx.World && ctx.World->Document() && ctx.World->Document()->MainPart())
    {
      Material material = ctx.WoodAlbedoPath.empty()
                              ? Material{}
                              : MakeWoodMaterial(ctx.WoodAlbedoPath);
      ctx.World->SyncPartBodies(*ctx.World->Document()->MainPart(),
                                std::move(material));
    }
    if (ctx.RequestRedraw)
    {
      ctx.RequestRedraw();
    }
    if (ctx.RefreshUi)
    {
      ctx.RefreshUi();
    }
    const QString msg = TrCmd("Undone: %1").arg(label);
    if (ctx.ReportStatus) ctx.ReportStatus(msg);
    return CommandResult::Ok(msg);
  }
};

class RedoCommand final : public ICommand
{
 public:
  [[nodiscard]] std::string_view id() const noexcept override
  {
    return "edit.redo";
  }
  [[nodiscard]] std::string_view title() const noexcept override
  {
    return "Redo";
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override
  {
    return ctx.History && ctx.History->can_redo();
  }

  CommandResult execute(CommandContext& ctx) override
  {
    const QString label = ctx.History->redo_label();
    if (!ctx.History->redo())
    {
      return CommandResult::Failed(TrCmd("Nothing to redo"));
    }
    if (ctx.World && ctx.World->Document() && ctx.World->Document()->MainPart())
    {
      Material material = ctx.WoodAlbedoPath.empty()
                              ? Material{}
                              : MakeWoodMaterial(ctx.WoodAlbedoPath);
      ctx.World->SyncPartBodies(*ctx.World->Document()->MainPart(),
                                std::move(material));
    }
    if (ctx.RequestRedraw)
    {
      ctx.RequestRedraw();
    }
    if (ctx.RefreshUi)
    {
      ctx.RefreshUi();
    }
    const QString msg = TrCmd("Redone: %1").arg(label);
    if (ctx.ReportStatus) ctx.ReportStatus(msg);
    return CommandResult::Ok(msg);
  }
};

template <boolean::BooleanOp Op>
class BooleanOpCommand final : public ICommand
{
 public:
  [[nodiscard]] std::string_view id() const noexcept override
  {
    if constexpr (Op == boolean::BooleanOp::Union)
    {
      return "boolean.union";
    }
    if constexpr (Op == boolean::BooleanOp::Subtract)
    {
      return "boolean.subtract";
    }
    return "boolean.intersect";
  }

  [[nodiscard]] std::string_view title() const noexcept override
  {
    if constexpr (Op == boolean::BooleanOp::Union)
    {
      return "Boolean Union";
    }
    if constexpr (Op == boolean::BooleanOp::Subtract)
    {
      return "Boolean Subtract";
    }
    return "Boolean Intersect";
  }

  [[nodiscard]] CommandKind kind() const noexcept override
  {
    return CommandKind::Interactive;
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override
  {
    return ctx.World != nullptr && ctx.World->Document() != nullptr &&
           ctx.World->Document()->MainPart() != nullptr;
  }

  [[nodiscard]] std::unique_ptr<ITool> make_tool(
      CommandContext& /*ctx*/) const override
  {
    return std::make_unique<BooleanTwoBodyTool>(Op);
  }

  CommandResult execute(CommandContext& ctx) override
  {
    if (!ctx.Scene || !ctx.World)
    {
      return CommandResult::Failed(TrCmd("Invalid context"));
    }
    auto& registry = ctx.World->registry();
    auto operands = ResolveBooleanOperands(registry, *ctx.Scene);
    if (!operands)
    {
      return CommandResult::Failed(
          TrCmd("Cannot resolve boolean operands (need two distinct features)"));
    }
    const QString histLabel = QString::fromUtf8(
        title().data(), static_cast<int>(title().size()));
    return ExecuteBoolean(ctx, Op, operands->Target, operands->Tool,
                          histLabel);
  }
};

template <class Cmd>
void add(CommandRegistry& registry)
{
  registry.register_command(std::string(Cmd{}.id()),
                            [] { return std::make_unique<Cmd>(); });
}

}  // namespace

void register_builtin_commands(CommandRegistry& registry)
{
  add<NewDocumentCommand>(registry);
  add<OpenXlCommand>(registry);
  add<SaveXlCommand>(registry);
  add<ExportDxfCommand>(registry);
  add<CreateBoxCommand>(registry);
  add<CreateSphereCommand>(registry);
  add<CreateBezierCommand>(registry);
  add<CreateNurbsCurveCommand>(registry);
  add<ExtrudePadCommand>(registry);
  add<ElevateBezierCommand>(registry);
  add<CreateBoxInstantCommand>(registry);
  add<CopyCommand>(registry);
  add<MoveCommand>(registry);
  add<DeleteSelectionCommand>(registry);
  add<BooleanOpCommand<boolean::BooleanOp::Union>>(registry);
  add<BooleanOpCommand<boolean::BooleanOp::Subtract>>(registry);
  add<BooleanOpCommand<boolean::BooleanOp::Intersect>>(registry);
  add<UndoCommand>(registry);
  add<RedoCommand>(registry);
  BREP_INFO("registered builtin commands: {}", registry.ids().size());
}

CommandResult open_xl_file(CommandContext& ctx, const QString& path)
{
  if (!ctx.World || !ctx.Session)
{
    return CommandResult::Failed(TrCmd("Cannot open: invalid context"));
  }
  if (path.isEmpty())
  {
    return CommandResult::Failed(TrCmd("Path is empty"));
  }

  if (!ctx.DocumentService)
  {
    return CommandResult::Failed(TrCmd("Invalid context"));
  }

  auto loaded = ctx.DocumentService->load(path.toStdString());
  if (!loaded.ok)
  {
    const QString err = QString::fromStdString(loaded.error);
    if (ctx.ParentWidget)
    {
      QMessageBox::critical(ctx.ParentWidget, TrCmd("Open failed"), err);
    }
    return CommandResult::Failed(err);
  }

  if (ctx.History) ctx.History->clear();
  Material material = ctx.WoodAlbedoPath.empty()
                          ? Material{}
                          : MakeWoodMaterial(ctx.WoodAlbedoPath);

  brep::io::BodyMeshCache mesh_cache;
  const brep::io::BodyMeshCache* cache_ptr = nullptr;
  auto cache_loaded = ctx.DocumentService->load_mesh_cache(
      path.toStdString(), loaded.document->Guid);
  if (cache_loaded.ok)
  {
    mesh_cache = std::move(cache_loaded.cache);
    cache_ptr = &mesh_cache;
  }

  ctx.World->AdoptDocument(std::move(loaded.document), std::move(material),
                            cache_ptr);
  ctx.Session->set_document_path(path);
  if (ctx.AfterDocumentReset) ctx.AfterDocumentReset();
  if (ctx.RequestRedraw) ctx.RequestRedraw();
  if (ctx.RefreshUi) ctx.RefreshUi();
  const QString msg = TrCmd("Opened: %1").arg(path);
  if (ctx.ReportStatus) ctx.ReportStatus(msg);
  return CommandResult::Ok(msg);
}

}  // namespace brep::viewer::commands
