#include "commands/CommandRegistry.h"
#include "commands/DocumentHistory.h"
#include "commands/ITool.h"
#include "commands/tools/CopyTool.h"
#include "commands/tools/CreateBoxTool.h"
#include "commands/tools/CreateSphereTool.h"
#include "ecs/Systems.h"

#include "adapter/DocumentService.h"
#include "adapter/SceneAdapter.h"
#include "api/Core.h"
#include "api/Modeling.h"
#include "ecs/Components.h"
#include "io/DxfExport.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

#include <array>
#include <optional>
#include <vector>

namespace brep::viewer::commands
{
namespace
{

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
    const QString msg = QStringLiteral("???????");
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
    return ctx.World != nullptr && ctx.World->document() != nullptr &&
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
          ctx.ParentWidget, QStringLiteral("????"), start,
          QStringLiteral("XCAD Document (*.xl);;All Files (*)"));
      if (path.isEmpty())
      {
        return CommandResult::Cancelled(QStringLiteral("?????"));
      }
      if (!path.endsWith(QStringLiteral(".xl"), Qt::CaseInsensitive))
      {
        path += QStringLiteral(".xl");
      }
    }

    auto result =
        adapter::DocumentService{}.save(*ctx.World->document(), path.toStdString());
    if (!result.ok)
    {
      const QString err = QString::fromStdString(result.error);
      if (ctx.ParentWidget)
      {
        QMessageBox::critical(ctx.ParentWidget, QStringLiteral("????"),
                              err);
      }
      return CommandResult::Failed(err);
    }

    ctx.World->document()->SetPath(path.toStdString());
    ctx.World->document()->MarkClean();
    ctx.Session->set_document_path(path);
    if (ctx.RefreshUi) ctx.RefreshUi();
    const QString msg = QStringLiteral("???: %1").arg(path);
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
        ctx.ParentWidget, QStringLiteral("????"), start,
        QStringLiteral("XCAD Document (*.xl);;All Files (*)"));
    if (path.isEmpty())
    {
      return CommandResult::Cancelled(QStringLiteral("?????"));
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
        ctx.ParentWidget, QStringLiteral("?? DWG/DXF"), start,
        QStringLiteral("CAD Drawing (*.dxf);;All Files (*)"));
    if (path.isEmpty())
    {
      return CommandResult::Cancelled(QStringLiteral("?????"));
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
        QMessageBox::critical(ctx.ParentWidget, QStringLiteral("????"),
                              err);
      }
      return CommandResult::Failed(err);
    }

    ctx.Session->set_export_path(path);
    if (ctx.RefreshUi) ctx.RefreshUi();
    const QString msg = QStringLiteral("??? DXF ???%1 ??: %2")
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
    return ctx.World != nullptr && ctx.World->document() != nullptr &&
           ctx.World->document()->MainPart() != nullptr;
  }

  [[nodiscard]] std::unique_ptr<ITool> make_tool(
      CommandContext& /*ctx*/) const override
  {
    return std::make_unique<CopyTool>();
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
    return ctx.World != nullptr && ctx.World->document() != nullptr &&
           ctx.World->document()->MainPart() != nullptr;
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
    return ctx.World != nullptr && ctx.World->document() != nullptr &&
           ctx.World->document()->MainPart() != nullptr;
  }

  [[nodiscard]] std::unique_ptr<ITool> make_tool(
      CommandContext& /*ctx*/) const override
  {
    return std::make_unique<CreateSphereTool>();
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
    return ctx.World != nullptr && ctx.World->document() != nullptr &&
           ctx.World->document()->MainPart() != nullptr;
  }

  CommandResult execute(CommandContext& ctx) override
  {
    using namespace brep;
    adapter::SceneAdapter scene(ctx.World->document());
    Part* part = scene.main_part();
    if (!part)
    {
      return CommandResult::Failed(QStringLiteral("???? Part"));
    }
    BoxSpec spec{
        .Min = Point3d{0, 0, 0},
        .Max = Point3d{2, 1, 3},
        .Name = "box",
    };
    Body* body = scene.add_box(spec);
    if (!body)
    {
      return CommandResult::Failed(QStringLiteral("??????"));
    }

    Guid feature_guid{};
    if (auto obj = scene.object_for_body(body->Guid))
    {
      feature_guid = obj->feature_guid;
      scene.record_append_feature(feat::FeatureId{feature_guid}, spec);
    }

    Material material = ctx.WoodAlbedoPath.empty()
                            ? Material{}
                            : MakeWoodMaterial(ctx.WoodAlbedoPath);
    auto mesh = scene.mesh_for_body(body->Guid);
    ctx.World->create_body_renderable(body->Name, body->Guid,
                                      std::move(mesh.faces),
                                      std::move(mesh.edges), material,
                                      Point3d{}, feature_guid);
    if (ctx.Session) ctx.Session->mark_dirty();
    if (ctx.RequestRedraw) ctx.RequestRedraw();

    const std::string wood = ctx.WoodAlbedoPath;
    ecs::World* world = ctx.World;
    Part* part_ptr = part;
    if (ctx.History)
    {
      ctx.History->push({
          .label = QStringLiteral("????"),
          .undo =
              [world, part_ptr, wood, session = ctx.Session,
               redraw = ctx.RequestRedraw, refresh = ctx.RefreshUi] {
                if (!world || !part_ptr) return;
                adapter::SceneAdapter scene_u(world->document());
                scene_u.undo_feature();
                Material mat =
                    wood.empty() ? Material{} : MakeWoodMaterial(wood);
                world->sync_part_bodies(*part_ptr, std::move(mat));
                if (session) session->mark_dirty();
                if (redraw) redraw();
                if (refresh) refresh();
              },
          .redo =
              [world, part_ptr, wood, session = ctx.Session,
               redraw = ctx.RequestRedraw, refresh = ctx.RefreshUi] {
                if (!world || !part_ptr) return;
                adapter::SceneAdapter scene_r(world->document());
                scene_r.redo_feature();
                Material mat =
                    wood.empty() ? Material{} : MakeWoodMaterial(wood);
                world->sync_part_bodies(*part_ptr, std::move(mat));
                if (session) session->mark_dirty();
                if (redraw) redraw();
                if (refresh) refresh();
              },
      });
    }

    if (ctx.RefreshUi) ctx.RefreshUi();
    const QString msg =
        QStringLiteral("??? Body '%1'")
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
    return ctx.World != nullptr && ctx.World->document() != nullptr &&
           ctx.World->document()->MainPart() != nullptr &&
           ecs::selected_count(ctx.World->registry()) > 0;
  }

  CommandResult execute(CommandContext& ctx) override
  {
    adapter::SceneAdapter scene(ctx.World->document());
    Part* part = scene.main_part();
    if (!part)
    {
      return CommandResult::Failed(QStringLiteral("???? Part"));
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
        feature_guid = fref->feature_guid;
      }
      if (const auto* body = registry.try_get<ecs::BodyRef>(entity))
      {
        body_guid = body->guid;
      }
      if (auto fid = scene.feature_id_for(feature_guid, body_guid))
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
      return CommandResult::Failed(QStringLiteral("????????"));
    }

    int removed = 0;
    for (const auto& fid : to_remove)
    {
      if (scene.remove_feature(fid)) ++removed;
    }
    if (removed == 0)
    {
      return CommandResult::Failed(QStringLiteral("????"));
    }

    Material material = ctx.WoodAlbedoPath.empty()
                            ? Material{}
                            : MakeWoodMaterial(ctx.WoodAlbedoPath);
    ctx.World->sync_part_bodies(*part, std::move(material));
    if (ctx.Session) ctx.Session->mark_dirty();
    if (ctx.RequestRedraw) ctx.RequestRedraw();

    const std::string wood = ctx.WoodAlbedoPath;
    ecs::World* world = ctx.World;
    Part* part_ptr = part;
    const int undo_steps = removed;
    if (ctx.History)
    {
      ctx.History->push({
          .label = QStringLiteral("?? %1 ???").arg(removed),
          .undo =
              [world, part_ptr, wood, undo_steps, session = ctx.Session,
               redraw = ctx.RequestRedraw, refresh = ctx.RefreshUi] {
                if (!world || !part_ptr) return;
                adapter::SceneAdapter scene_u(world->document());
                scene_u.undo_feature(undo_steps);
                Material mat =
                    wood.empty() ? Material{} : MakeWoodMaterial(wood);
                world->sync_part_bodies(*part_ptr, std::move(mat));
                if (session) session->mark_dirty();
                if (redraw) redraw();
                if (refresh) refresh();
              },
          .redo =
              [world, part_ptr, wood, undo_steps, session = ctx.Session,
               redraw = ctx.RequestRedraw, refresh = ctx.RefreshUi] {
                if (!world || !part_ptr) return;
                adapter::SceneAdapter scene_r(world->document());
                scene_r.redo_feature(undo_steps);
                Material mat =
                    wood.empty() ? Material{} : MakeWoodMaterial(wood);
                world->sync_part_bodies(*part_ptr, std::move(mat));
                if (session) session->mark_dirty();
                if (redraw) redraw();
                if (refresh) refresh();
              },
      });
    }

    if (ctx.RefreshUi) ctx.RefreshUi();
    const QString msg = QStringLiteral("??? %1 ???").arg(removed);
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
      return CommandResult::Failed(QStringLiteral("????????"));
    }
    if (ctx.RefreshUi) ctx.RefreshUi();
    const QString msg = QStringLiteral("???: %1").arg(label);
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
      return CommandResult::Failed(QStringLiteral("????????"));
    }
    if (ctx.RefreshUi) ctx.RefreshUi();
    const QString msg = QStringLiteral("???: %1").arg(label);
    if (ctx.ReportStatus) ctx.ReportStatus(msg);
    return CommandResult::Ok(msg);
  }
};

struct BooleanOperands
{
  feat::FeatureId target{};
  feat::FeatureId tool{};
};

[[nodiscard]] std::optional<BooleanOperands> resolve_boolean_operands(
    entt::registry& registry, adapter::SceneAdapter& scene)
{
  if (ecs::selected_count(registry) != 2) return std::nullopt;

  const entt::entity primary_ent = ecs::selected_entity(registry);
  std::array<std::optional<feat::FeatureId>, 2> ids{};
  int n = 0;
  for (auto entity : registry.view<ecs::SelectedTag>())
  {
    Guid feature_guid{};
    Guid body_guid{};
    if (const auto* fref = registry.try_get<ecs::FeatureRef>(entity))
    {
      feature_guid = fref->feature_guid;
    }
    if (const auto* body = registry.try_get<ecs::BodyRef>(entity))
    {
      body_guid = body->guid;
    }
    auto fid = scene.feature_id_for(feature_guid, body_guid);
    if (!fid || n >= 2) return std::nullopt;
    ids[static_cast<std::size_t>(n++)] = *fid;
  }
  if (n != 2 || !ids[0] || !ids[1] || *ids[0] == *ids[1]) return std::nullopt;

  Guid primary_fg{};
  Guid primary_bg{};
  if (primary_ent != entt::null)
  {
    if (const auto* fref = registry.try_get<ecs::FeatureRef>(primary_ent))
  {
      primary_fg = fref->feature_guid;
    }
    if (const auto* body = registry.try_get<ecs::BodyRef>(primary_ent))
    {
      primary_bg = body->guid;
    }
  }
  auto target = scene.feature_id_for(primary_fg, primary_bg);
  if (!target)
  {
    // Fallback: first selected as target.
    target = ids[0];
  }
  const feat::FeatureId tool =
      (*ids[0] == *target) ? *ids[1] : *ids[0];
  if (tool == *target) return std::nullopt;
  return BooleanOperands{*target, tool};
}

[[nodiscard]] const char* boolean_result_name(boolean::BooleanOp op) noexcept
{
  switch (op)
{
    case boolean::BooleanOp::Union:
      return "Fuse";
    case boolean::BooleanOp::Subtract:
      return "Cut";
    case boolean::BooleanOp::Intersect:
      return "Common";
  }
  return "Boolean";
}

template <boolean::BooleanOp Op>
class BooleanOpCommand final : public ICommand
{
 public:
  [[nodiscard]] std::string_view id() const noexcept override
  {
    if constexpr (Op == boolean::BooleanOp::Union)
  {
      return "boolean.union";
    } else if constexpr (Op == boolean::BooleanOp::Subtract)
    {
      return "boolean.subtract";
    }
    else
    {
      return "boolean.intersect";
    }
  }

  [[nodiscard]] std::string_view title() const noexcept override
  {
    if constexpr (Op == boolean::BooleanOp::Union)
  {
      return "Boolean Union";
    } else if constexpr (Op == boolean::BooleanOp::Subtract)
    {
      return "Boolean Subtract";
    }
    else
    {
      return "Boolean Intersect";
    }
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override
  {
    return ctx.World != nullptr && ctx.World->document() != nullptr &&
           ctx.World->document()->MainPart() != nullptr &&
           ecs::selected_count(ctx.World->registry()) == 2;
  }

  CommandResult execute(CommandContext& ctx) override
  {
    adapter::SceneAdapter scene(ctx.World->document());
    Part* part = scene.main_part();
    if (!part)
    {
      return CommandResult::Failed(QStringLiteral("???? Part"));
    }

    auto& registry = ctx.World->registry();
    if (ecs::selected_count(registry) != 2)
    {
      return CommandResult::Failed(
          QStringLiteral("????? 2 ??????????"));
    }

    auto operands = resolve_boolean_operands(registry, scene);
    if (!operands)
    {
      return CommandResult::Failed(
          QStringLiteral("???????????????????"));
    }

    Body* result = scene.add_boolean(Op, operands->target, operands->tool,
                                     boolean_result_name(Op));
    if (!result)
    {
      return CommandResult::Failed(QStringLiteral("??????"));
    }

    Material material = ctx.WoodAlbedoPath.empty()
                            ? Material{}
                            : MakeWoodMaterial(ctx.WoodAlbedoPath);
    ctx.World->sync_part_bodies(*part, std::move(material));
    ecs::clear_selection(registry);
    if (ctx.Session) ctx.Session->mark_dirty();
    if (ctx.RequestRedraw) ctx.RequestRedraw();

    const std::string wood = ctx.WoodAlbedoPath;
    ecs::World* world = ctx.World;
    Part* part_ptr = part;
    const QString hist_label = QString::fromUtf8(title().data(),
                                                 static_cast<int>(title().size()));
    if (ctx.History)
    {
      ctx.History->push({
          .label = hist_label,
          .undo =
              [world, part_ptr, wood, session = ctx.Session,
               redraw = ctx.RequestRedraw, refresh = ctx.RefreshUi] {
                if (!world || !part_ptr) return;
                adapter::SceneAdapter scene_u(world->document());
                scene_u.undo_feature();
                Material mat =
                    wood.empty() ? Material{} : MakeWoodMaterial(wood);
                world->sync_part_bodies(*part_ptr, std::move(mat));
                if (session) session->mark_dirty();
                if (redraw) redraw();
                if (refresh) refresh();
              },
          .redo =
              [world, part_ptr, wood, session = ctx.Session,
               redraw = ctx.RequestRedraw, refresh = ctx.RefreshUi] {
                if (!world || !part_ptr) return;
                adapter::SceneAdapter scene_r(world->document());
                scene_r.redo_feature();
                Material mat =
                    wood.empty() ? Material{} : MakeWoodMaterial(wood);
                world->sync_part_bodies(*part_ptr, std::move(mat));
                if (session) session->mark_dirty();
                if (redraw) redraw();
                if (refresh) refresh();
              },
      });
    }

    if (ctx.RefreshUi) ctx.RefreshUi();
    const QString msg =
        QStringLiteral("????: %1").arg(QString::fromStdString(result->Name));
    if (ctx.ReportStatus) ctx.ReportStatus(msg);
    return CommandResult::Ok(msg);
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
  add<CreateBoxInstantCommand>(registry);
  add<CopyCommand>(registry);
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
    return CommandResult::Failed(QStringLiteral("??????????"));
  }
  if (path.isEmpty())
  {
    return CommandResult::Failed(QStringLiteral("????"));
  }

  adapter::DocumentService docs;
  auto loaded = docs.load(path.toStdString());
  if (!loaded.ok)
  {
    const QString err = QString::fromStdString(loaded.error);
    if (ctx.ParentWidget)
    {
      QMessageBox::critical(ctx.ParentWidget, QStringLiteral("????"), err);
    }
    return CommandResult::Failed(err);
  }

  if (ctx.History) ctx.History->clear();
  Material material = ctx.WoodAlbedoPath.empty()
                          ? Material{}
                          : MakeWoodMaterial(ctx.WoodAlbedoPath);

  brep::io::BodyMeshCache mesh_cache;
  const brep::io::BodyMeshCache* cache_ptr = nullptr;
  auto cache_loaded =
      docs.load_mesh_cache(path.toStdString(), loaded.document->Guid);
  if (cache_loaded.ok)
  {
    mesh_cache = std::move(cache_loaded.cache);
    cache_ptr = &mesh_cache;
  }

  ctx.World->adopt_document(std::move(loaded.document), std::move(material),
                            cache_ptr);
  ctx.Session->set_document_path(path);
  if (ctx.AfterDocumentReset) ctx.AfterDocumentReset();
  if (ctx.RequestRedraw) ctx.RequestRedraw();
  if (ctx.RefreshUi) ctx.RefreshUi();
  const QString msg = QStringLiteral("???: %1").arg(path);
  if (ctx.ReportStatus) ctx.ReportStatus(msg);
  return CommandResult::Ok(msg);
}

}  // namespace brep::viewer::commands
