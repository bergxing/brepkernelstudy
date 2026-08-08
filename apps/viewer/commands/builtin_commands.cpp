#include "commands/command_registry.hpp"
#include "commands/document_history.hpp"
#include "commands/itool.hpp"
#include "commands/tools/create_box_tool.hpp"

#include "brep/brep.hpp"
#include "brep/log.hpp"
#include "ecs/components.hpp"
#include "io/dxf_export.hpp"

#include <QDir>
#include <QFileDialog>
#include <QMessageBox>

#include <vector>

namespace brep::viewer::commands {
namespace {

class NewDocumentCommand final : public ICommand {
 public:
  [[nodiscard]] std::string_view id() const noexcept override {
    return "doc.new";
  }
  [[nodiscard]] std::string_view title() const noexcept override {
    return "New Document";
  }
  [[nodiscard]] CommandKind kind() const noexcept override {
    return CommandKind::Instant;
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override {
    return ctx.world != nullptr && ctx.session != nullptr;
  }

  CommandResult execute(CommandContext& ctx) override {
    if (ctx.history) ctx.history->clear();
    ctx.session->new_blank_document(*ctx.world);
    if (ctx.after_document_reset) ctx.after_document_reset();
    if (ctx.request_redraw) ctx.request_redraw();
    if (ctx.refresh_ui) ctx.refresh_ui();
    const QString msg = QStringLiteral("已新建空白文档");
    if (ctx.report_status) ctx.report_status(msg);
    return CommandResult::ok(msg);
  }
};

class ExportDxfCommand final : public ICommand {
 public:
  [[nodiscard]] std::string_view id() const noexcept override {
    return "file.export_dxf";
  }
  [[nodiscard]] std::string_view title() const noexcept override {
    return "Export DXF";
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override {
    return ctx.world != nullptr && ctx.session != nullptr;
  }

  CommandResult execute(CommandContext& ctx) override {
    QString start = ctx.session->path();
    if (start.isEmpty()) {
      start = QDir::homePath() + QStringLiteral("/untitled.dxf");
    }

    QString path = QFileDialog::getSaveFileName(
        ctx.parent_widget, QStringLiteral("导出 DWG/DXF"), start,
        QStringLiteral("CAD Drawing (*.dxf);;All Files (*)"));
    if (path.isEmpty()) {
      return CommandResult::cancelled(QStringLiteral("已取消导出"));
    }
    if (!path.endsWith(QStringLiteral(".dxf"), Qt::CaseInsensitive)) {
      path += QStringLiteral(".dxf");
    }

    std::vector<io::DxfSegment> segments;
    auto view =
        ctx.world->registry().view<ecs::MeshComponent, ecs::Transform>();
    for (auto entity : view) {
      const auto& mesh = view.get<ecs::MeshComponent>(entity);
      const auto& xform = view.get<ecs::Transform>(entity);
      io::append_edge_segments(mesh.edges, xform.position, segments);
    }

    if (!io::write_edges_dxf(path.toStdString(), segments)) {
      const QString err = QString::fromStdString(io::last_dxf_error());
      if (ctx.parent_widget) {
        QMessageBox::critical(ctx.parent_widget, QStringLiteral("导出失败"),
                              err);
      }
      return CommandResult::failed(err);
    }

    ctx.session->set_export_path(path);
    if (ctx.refresh_ui) ctx.refresh_ui();
    const QString msg = QStringLiteral("已导出 DXF 线框（%1 段）: %2")
                            .arg(segments.size())
                            .arg(path);
    if (ctx.report_status) ctx.report_status(msg);
    return CommandResult::ok(msg);
  }
};

/// Interactive two-click box (Phase 3 tool).
class CreateBoxCommand final : public ICommand {
 public:
  [[nodiscard]] std::string_view id() const noexcept override {
    return "part.create_box";
  }
  [[nodiscard]] std::string_view title() const noexcept override {
    return "Create Box (interactive)";
  }
  [[nodiscard]] CommandKind kind() const noexcept override {
    return CommandKind::Interactive;
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override {
    return ctx.world != nullptr && ctx.world->document() != nullptr &&
           ctx.world->document()->main_part() != nullptr;
  }

  [[nodiscard]] std::unique_ptr<ITool> make_tool(
      CommandContext& /*ctx*/) const override {
    return std::make_unique<CreateBoxTool>();
  }
};

/// One-shot default-size box (keeps a quick path).
class CreateBoxInstantCommand final : public ICommand {
 public:
  [[nodiscard]] std::string_view id() const noexcept override {
    return "part.create_box_instant";
  }
  [[nodiscard]] std::string_view title() const noexcept override {
    return "Create Box (instant)";
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override {
    return ctx.world != nullptr && ctx.world->document() != nullptr &&
           ctx.world->document()->main_part() != nullptr;
  }

  CommandResult execute(CommandContext& ctx) override {
    using namespace brep;
    Part* part = ctx.world->document()->main_part();
    BoxSpec spec{
        .min = Point3d{0, 0, 0},
        .max = Point3d{2, 1, 3},
        .name = "box",
    };
    Body* body = part->add_box(spec);
    if (!body) {
      return CommandResult::failed(QStringLiteral("创建盒子失败"));
    }

    Guid feature_guid{};
    if (const auto* feature = part->features().find_by_body(body->guid)) {
      feature_guid = feature->id().guid;
      feat::FeatureTransaction tx;
      tx.kind = feat::TxKind::AppendFeature;
      tx.feature = feature->id();
      tx.feature_type = "Box";
      tx.box_spec = spec;
      part->feature_history().record(std::move(tx));
    }

    Material material = ctx.wood_albedo_path.empty()
                            ? Material{}
                            : make_wood_material(ctx.wood_albedo_path);
    ctx.world->create_body_renderable(body->name, body->guid,
                                      tessellate_body(*body),
                                      extract_edges(*body), material,
                                      Point3d{}, feature_guid);
    if (ctx.session) ctx.session->mark_dirty();
    if (ctx.request_redraw) ctx.request_redraw();

    const std::string wood = ctx.wood_albedo_path;
    ecs::World* world = ctx.world;
    Part* part_ptr = part;
    if (ctx.history) {
      ctx.history->push({
          .label = QStringLiteral("创建盒子"),
          .undo =
              [world, part_ptr, wood, session = ctx.session,
               redraw = ctx.request_redraw, refresh = ctx.refresh_ui] {
                if (!world || !part_ptr) return;
                part_ptr->feature_history().undo(*part_ptr);
                Material mat =
                    wood.empty() ? Material{} : make_wood_material(wood);
                world->sync_part_bodies(*part_ptr, std::move(mat));
                if (session) session->mark_dirty();
                if (redraw) redraw();
                if (refresh) refresh();
              },
          .redo =
              [world, part_ptr, wood, session = ctx.session,
               redraw = ctx.request_redraw, refresh = ctx.refresh_ui] {
                if (!world || !part_ptr) return;
                part_ptr->feature_history().redo(*part_ptr);
                Material mat =
                    wood.empty() ? Material{} : make_wood_material(wood);
                world->sync_part_bodies(*part_ptr, std::move(mat));
                if (session) session->mark_dirty();
                if (redraw) redraw();
                if (refresh) refresh();
              },
      });
    }

    if (ctx.refresh_ui) ctx.refresh_ui();
    const QString msg =
        QStringLiteral("已创建 Body '%1'")
            .arg(QString::fromStdString(body->guid.to_string()));
    if (ctx.report_status) ctx.report_status(msg);
    return CommandResult::ok(msg);
  }
};

class UndoCommand final : public ICommand {
 public:
  [[nodiscard]] std::string_view id() const noexcept override {
    return "edit.undo";
  }
  [[nodiscard]] std::string_view title() const noexcept override {
    return "Undo";
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override {
    return ctx.history && ctx.history->can_undo();
  }

  CommandResult execute(CommandContext& ctx) override {
    const QString label = ctx.history->undo_label();
    if (!ctx.history->undo()) {
      return CommandResult::failed(QStringLiteral("没有可撤销的操作"));
    }
    if (ctx.refresh_ui) ctx.refresh_ui();
    const QString msg = QStringLiteral("已撤销: %1").arg(label);
    if (ctx.report_status) ctx.report_status(msg);
    return CommandResult::ok(msg);
  }
};

class RedoCommand final : public ICommand {
 public:
  [[nodiscard]] std::string_view id() const noexcept override {
    return "edit.redo";
  }
  [[nodiscard]] std::string_view title() const noexcept override {
    return "Redo";
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override {
    return ctx.history && ctx.history->can_redo();
  }

  CommandResult execute(CommandContext& ctx) override {
    const QString label = ctx.history->redo_label();
    if (!ctx.history->redo()) {
      return CommandResult::failed(QStringLiteral("没有可重做的操作"));
    }
    if (ctx.refresh_ui) ctx.refresh_ui();
    const QString msg = QStringLiteral("已重做: %1").arg(label);
    if (ctx.report_status) ctx.report_status(msg);
    return CommandResult::ok(msg);
  }
};

template <class Cmd>
void add(CommandRegistry& registry) {
  registry.register_command(std::string(Cmd{}.id()),
                            [] { return std::make_unique<Cmd>(); });
}

}  // namespace

void register_builtin_commands(CommandRegistry& registry) {
  add<NewDocumentCommand>(registry);
  add<ExportDxfCommand>(registry);
  add<CreateBoxCommand>(registry);
  add<CreateBoxInstantCommand>(registry);
  add<UndoCommand>(registry);
  add<RedoCommand>(registry);
  BREP_INFO("registered builtin commands: {}", registry.ids().size());
}

}  // namespace brep::viewer::commands
