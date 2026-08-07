#include "commands/command_registry.hpp"

#include "brep/brep.hpp"
#include "brep/log.hpp"
#include "ecs/components.hpp"
#include "io/dxf_export.hpp"

#include <QDir>
#include <QFileDialog>
#include <QMessageBox>

#include <filesystem>
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

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override {
    return ctx.world != nullptr && ctx.session != nullptr;
  }

  CommandResult execute(CommandContext& ctx) override {
    ctx.session->new_blank_document(*ctx.world);
    if (ctx.after_document_reset) ctx.after_document_reset();
    if (ctx.request_redraw) ctx.request_redraw();
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
    const QString msg = QStringLiteral("已导出 DXF 线框（%1 段）: %2")
                            .arg(segments.size())
                            .arg(path);
    if (ctx.report_status) ctx.report_status(msg);
    return CommandResult::ok(msg);
  }
};

class CreateBoxCommand final : public ICommand {
 public:
  [[nodiscard]] std::string_view id() const noexcept override {
    return "part.create_box";
  }
  [[nodiscard]] std::string_view title() const noexcept override {
    return "Create Box";
  }

  [[nodiscard]] bool can_execute(const CommandContext& ctx) const override {
    return ctx.world != nullptr && ctx.world->document() != nullptr &&
           ctx.world->document()->main_part() != nullptr;
  }

  CommandResult execute(CommandContext& ctx) override {
    using namespace brep;
    Part* part = ctx.world->document()->main_part();
    if (!part) {
      return CommandResult::failed(QStringLiteral("当前没有 Part"));
    }

    Body* body = part->add_box(BoxSpec{
        .min = Point3d{0, 0, 0},
        .max = Point3d{2, 1, 3},
        .name = "box",
    });

    Material material = ctx.wood_albedo_path.empty()
                            ? Material{}
                            : make_wood_material(ctx.wood_albedo_path);

    ctx.world->create_renderable(body->name, tessellate_body(*body),
                                 extract_edges(*body), std::move(material),
                                 Point3d{0, 0, 0});

    if (ctx.session) ctx.session->mark_dirty();
    if (ctx.request_redraw) ctx.request_redraw();

    const QString msg =
        QStringLiteral("已创建 Body '%1' (%2)")
            .arg(QString::fromStdString(body->name))
            .arg(QString::fromStdString(body->guid.to_string()));
    if (ctx.report_status) ctx.report_status(msg);
    BREP_INFO("part.create_box guid={}", body->guid.to_string());
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
  BREP_INFO("registered builtin commands: {}", registry.ids().size());
}

}  // namespace brep::viewer::commands
