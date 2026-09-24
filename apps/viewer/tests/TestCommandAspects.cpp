#include "bootstrap/ViewerRuntimeServices.h"
#include "commands/CommandManager.h"
#include "commands/CommandPayload.h"
#include "commands/StatusAspect.h"

#include "brep/Aspect.h"

#include <QApplication>
#include <QString>

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace brep::viewer::commands
{
namespace
{

void EnsureQtApp()
{
    static int argc = 1;
    static char name[] = "viewer_command_tests";
    static char* argv[] = {name, nullptr};
    if (QApplication::instance() == nullptr)
    {
        static QApplication app(argc, argv);
    }
}

class RecordingAspect final : public brep::IAspect
{
 public:
    explicit RecordingAspect(std::string name, std::vector<std::string>* log)
        : m_name(std::move(name))
        , m_log(log)
    {
    }

    std::string_view Name() const noexcept override
    {
        return m_name;
    }

    void Before(const brep::AspectEvent& event) override
    {
        m_log->push_back(m_name + ":Before:" + std::string(event.Site));
    }

    void After(const brep::AspectEvent& event) override
    {
        std::string line = m_name + ":After:" + std::string(event.Site);
        if (event.Failed)
        {
            line += ":Failed";
        }
        const auto* payload = static_cast<const CommandPayload*>(event.Payload);
        if (payload != nullptr && payload->AnnounceCancel)
        {
            line += ":AnnounceCancel";
        }
        if (payload != nullptr && payload->Result != nullptr)
        {
            line += ":Status:" +
                    std::to_string(static_cast<int>(payload->Result->Status));
        }
        m_log->push_back(std::move(line));
    }

    void OnError(const brep::AspectEvent& event,
                 const std::exception& error) override
    {
        m_log->push_back(m_name + ":OnError:" + std::string(event.Site) + ":" +
                         error.what());
    }

 private:
    std::string m_name;
    std::vector<std::string>* m_log;
};

brep::AspectChain TwoRecorders(std::vector<std::string>* log)
{
    brep::AspectChain chain;
    chain.Add(std::make_unique<RecordingAspect>("A", log));
    chain.Add(std::make_unique<RecordingAspect>("B", log));
    return chain;
}

class OkCommand final : public ICommand
{
 public:
    std::string_view id() const noexcept override
    {
        return "test.ok";
    }
    std::string_view title() const noexcept override
    {
        return "Ok";
    }
    bool can_execute(const CommandContext&) const override
    {
        return true;
    }
    CommandResult execute(CommandContext&) override
    {
        return CommandResult::Ok("saved");
    }
};

class EmptyOkCommand final : public ICommand
{
 public:
    std::string_view id() const noexcept override
    {
        return "test.empty";
    }
    std::string_view title() const noexcept override
    {
        return "Empty";
    }
    bool can_execute(const CommandContext&) const override
    {
        return true;
    }
    CommandResult execute(CommandContext&) override
    {
        return CommandResult::Ok();
    }
};

class FailedCommand final : public ICommand
{
 public:
    std::string_view id() const noexcept override
    {
        return "test.fail";
    }
    std::string_view title() const noexcept override
    {
        return "Fail";
    }
    bool can_execute(const CommandContext&) const override
    {
        return true;
    }
    CommandResult execute(CommandContext&) override
    {
        return CommandResult::Failed("nope");
    }
};

class ThrowCommand final : public ICommand
{
 public:
    std::string_view id() const noexcept override
    {
        return "test.throw";
    }
    std::string_view title() const noexcept override
    {
        return "Throw";
    }
    bool can_execute(const CommandContext&) const override
    {
        return true;
    }
    CommandResult execute(CommandContext&) override
    {
        throw std::runtime_error("boom");
    }
};

class ClosedCommand final : public ICommand
{
 public:
    std::string_view id() const noexcept override
    {
        return "test.closed";
    }
    std::string_view title() const noexcept override
    {
        return "Closed";
    }
    bool can_execute(const CommandContext&) const override
    {
        return false;
    }
};

class ImmediateTool final : public ITool
{
 public:
    std::string_view Id() const noexcept override
    {
        return "test.tool";
    }
    QString Prompt() const override
    {
        return "pick";
    }
    void OnStart(CommandContext&) override
    {
        m_finished = true;
        m_result = CommandResult::Ok("done");
    }
    bool OnMousePress(CommandContext&, float, float, int) override
    {
        return false;
    }
    void OnMouseMove(CommandContext&, float, float) override {}
    void OnCancel(CommandContext&) override {}
    bool IsFinished() const noexcept override
    {
        return m_finished;
    }
    CommandResult Result() const override
    {
        return m_result;
    }

 private:
    bool m_finished{false};
    CommandResult m_result{CommandResult::Ok()};
};

class StickyTool final : public ITool
{
 public:
    std::string_view Id() const noexcept override
    {
        return "test.sticky";
    }
    QString Prompt() const override
    {
        return "stay";
    }
    void OnStart(CommandContext&) override {}
    bool OnMousePress(CommandContext&, float, float, int) override
    {
        return false;
    }
    void OnMouseMove(CommandContext&, float, float) override {}
    void OnCancel(CommandContext&) override {}
    bool OwnsUndoRedo() const noexcept override
    {
        return true;
    }
    bool UndoStep(CommandContext&) override
    {
        return true;
    }
    bool IsFinished() const noexcept override
    {
        return false;
    }
    CommandResult Result() const override
    {
        return CommandResult::Ok();
    }
};

class ImmediateCommand final : public ICommand
{
 public:
    std::string_view id() const noexcept override
    {
        return "test.tool";
    }
    std::string_view title() const noexcept override
    {
        return "Tool";
    }
    bool can_execute(const CommandContext&) const override
    {
        return true;
    }
    CommandKind kind() const noexcept override
    {
        return CommandKind::Interactive;
    }
    std::unique_ptr<ITool> make_tool(CommandContext&) const override
    {
        return std::make_unique<ImmediateTool>();
    }
};

class StickyCommand final : public ICommand
{
 public:
    std::string_view id() const noexcept override
    {
        return "test.sticky";
    }
    std::string_view title() const noexcept override
    {
        return "Sticky";
    }
    bool can_execute(const CommandContext&) const override
    {
        return true;
    }
    CommandKind kind() const noexcept override
    {
        return CommandKind::Interactive;
    }
    std::unique_ptr<ITool> make_tool(CommandContext&) const override
    {
        return std::make_unique<StickyTool>();
    }
};

class NullToolCommand final : public ICommand
{
 public:
    std::string_view id() const noexcept override
    {
        return "test.nulltool";
    }
    std::string_view title() const noexcept override
    {
        return "Null";
    }
    bool can_execute(const CommandContext&) const override
    {
        return true;
    }
    CommandKind kind() const noexcept override
    {
        return CommandKind::Interactive;
    }
};

TEST(CommandAspectChain, BeforeForwardAfterReverse)
{
    CommandRegistry registry;
    registry.register_command("test.ok", [] { return std::make_unique<OkCommand>(); });
    std::vector<std::string> log;
    CommandManager manager(registry, TwoRecorders(&log));
    CommandContext ctx;
    const CommandResult result = manager.run("test.ok", ctx);
    EXPECT_TRUE(result.Succeeded());
    ASSERT_EQ(log.size(), 4u);
    EXPECT_EQ(log[0], "A:Before:command.run");
    EXPECT_EQ(log[1], "B:Before:command.run");
    EXPECT_EQ(log[2], "B:After:command.run:Status:0");
    EXPECT_EQ(log[3], "A:After:command.run:Status:0");
}

TEST(CommandAspectChain, FailedResultUsesAfterNotOnError)
{
    CommandRegistry registry;
    registry.register_command("test.fail",
                              [] { return std::make_unique<FailedCommand>(); });
    std::vector<std::string> log;
    CommandManager manager(registry, TwoRecorders(&log));
    CommandContext ctx;
    const CommandResult result = manager.run("test.fail", ctx);
    EXPECT_EQ(result.Status, CommandStatus::Failed);
    EXPECT_NE(log.back().find("Failed"), std::string::npos);
    for (const std::string& line : log)
    {
        EXPECT_EQ(line.find("OnError"), std::string::npos);
    }
}

TEST(CommandAspectChain, ExceptionReversesOnErrorAndRethrows)
{
    CommandRegistry registry;
    registry.register_command("test.throw",
                              [] { return std::make_unique<ThrowCommand>(); });
    std::vector<std::string> log;
    CommandManager manager(registry, TwoRecorders(&log));
    CommandContext ctx;
    EXPECT_THROW(manager.run("test.throw", ctx), std::runtime_error);
    ASSERT_EQ(log.size(), 4u);
    EXPECT_EQ(log[2], "B:OnError:command.run:boom");
    EXPECT_EQ(log[3], "A:OnError:command.run:boom");
}

TEST(CommandAspectChain, EmptyChainReturnsCommandResult)
{
    CommandRegistry registry;
    registry.register_command("test.ok", [] { return std::make_unique<OkCommand>(); });
    CommandManager manager(registry, {});
    CommandContext ctx;
    EXPECT_EQ(manager.run("test.ok", ctx).Message, QString("saved"));
}

TEST(CommandAspectChain, PreconditionFailuresSkipAspects)
{
    CommandRegistry registry;
    registry.register_command("test.closed",
                              [] { return std::make_unique<ClosedCommand>(); });
    std::vector<std::string> log;
    CommandManager manager(registry, TwoRecorders(&log));
    CommandContext ctx;
    EXPECT_EQ(manager.run("missing", ctx).Status, CommandStatus::Failed);
    EXPECT_EQ(manager.run("test.closed", ctx).Status, CommandStatus::Failed);
    EXPECT_TRUE(log.empty());
}

TEST(CommandAspectChain, ToolStartThenImmediateFinish)
{
    CommandRegistry registry;
    registry.register_command("test.tool",
                              [] { return std::make_unique<ImmediateCommand>(); });
    std::vector<std::string> log;
    CommandManager manager(registry, TwoRecorders(&log));
    CommandContext ctx;
    EXPECT_TRUE(manager.run("test.tool", ctx).Succeeded());
    ASSERT_EQ(log.size(), 8u);
    EXPECT_EQ(log[0], "A:Before:command.tool.start");
    EXPECT_EQ(log[3], "A:After:command.tool.start");
    EXPECT_EQ(log[4], "A:Before:command.tool.finish");
    EXPECT_NE(log[7].find("command.tool.finish"), std::string::npos);
    EXPECT_EQ(log[7].find("command.run"), std::string::npos);
}

TEST(CommandAspectChain, UndoInsideToolSkipsAspects)
{
    CommandRegistry registry;
    registry.register_command("test.sticky",
                              [] { return std::make_unique<StickyCommand>(); });
    std::vector<std::string> log;
    CommandManager manager(registry, TwoRecorders(&log));
    CommandContext ctx;
    manager.run("test.sticky", ctx);
    log.clear();
    EXPECT_TRUE(manager.run("edit.undo", ctx).Succeeded());
    EXPECT_TRUE(log.empty());
}

TEST(CommandAspectChain, NullToolSkipsAspects)
{
    CommandRegistry registry;
    registry.register_command(
        "test.nulltool", [] { return std::make_unique<NullToolCommand>(); });
    std::vector<std::string> log;
    CommandManager manager(registry, TwoRecorders(&log));
    CommandContext ctx;
    EXPECT_EQ(manager.run("test.nulltool", ctx).Status, CommandStatus::Failed);
    EXPECT_TRUE(log.empty());
}

TEST(CommandAspectChain, MouseMoveIsNotAJoinPoint)
{
    CommandRegistry registry;
    registry.register_command("test.sticky",
                              [] { return std::make_unique<StickyCommand>(); });
    std::vector<std::string> log;
    CommandManager manager(registry, TwoRecorders(&log));
    CommandContext ctx;
    manager.run("test.sticky", ctx);
    log.clear();
    manager.tool_mouse_move(ctx, 0.0f, 0.0f);
    EXPECT_TRUE(log.empty());
}

TEST(CommandAspectChain, CancelAnnouncesWhenToolIsDropped)
{
    EnsureQtApp();
    CommandRegistry registry;
    registry.register_command("test.sticky",
                              [] { return std::make_unique<StickyCommand>(); });
    std::vector<std::string> log;
    CommandManager manager(registry, TwoRecorders(&log));
    CommandContext ctx;
    manager.run("test.sticky", ctx);
    log.clear();
    EXPECT_TRUE(manager.cancel_active_tool(ctx));
    ASSERT_EQ(log.size(), 4u);
    EXPECT_EQ(log[0], "A:Before:command.tool.cancel");
    EXPECT_EQ(log[2], "B:After:command.tool.cancel:AnnounceCancel");
    EXPECT_EQ(log[3], "A:After:command.tool.cancel:AnnounceCancel");
}

TEST(StatusAspectTest, ReportsMessageOnceAndCancelText)
{
    CommandRegistry registry;
    registry.register_command("test.ok", [] { return std::make_unique<OkCommand>(); });
    registry.register_command("test.empty",
                              [] { return std::make_unique<EmptyOkCommand>(); });
    registry.register_command("test.sticky",
                              [] { return std::make_unique<StickyCommand>(); });
    brep::AspectChain chain;
    chain.Add(std::make_unique<StatusAspect>());
    CommandManager manager(registry, std::move(chain));

    QStringList reports;
    CommandContext ctx;
    ctx.ReportStatus = [&](const QString& message) { reports.push_back(message); };
    manager.run("test.ok", ctx);
    ASSERT_EQ(reports.size(), 1);
    EXPECT_EQ(reports[0], QString("saved"));

    reports.clear();
    manager.run("test.empty", ctx);
    EXPECT_TRUE(reports.isEmpty());

    CommandContext bare;
    EXPECT_TRUE(manager.run("test.ok", bare).Succeeded());

    EnsureQtApp();
    reports.clear();
    manager.run("test.sticky", ctx);
    reports.clear();
    manager.cancel_active_tool(ctx);
    ASSERT_EQ(reports.size(), 1);
    EXPECT_EQ(reports[0], QString("已取消: test.sticky"));
}

std::vector<std::string>* g_info = nullptr;

void TraceInfo(bool isError, const char* message)
{
    if (!isError && g_info != nullptr && message != nullptr)
    {
        g_info->emplace_back(message);
    }
}

TEST(DefaultCommandAspectsTest, CreateCommandManagerInstallsFourInOrder)
{
    using brep::viewer::bootstrap::CreateCommandManager;
    using brep::viewer::bootstrap::ResolveCommandRegistry;
    ResolveCommandRegistry(nullptr).register_command(
        "test.ok", [] { return std::make_unique<OkCommand>(); });
    const std::unique_ptr<CommandManager> manager = CreateCommandManager(nullptr);
    const std::vector<std::string> names = manager->Names();
    ASSERT_EQ(names.size(), 4u);
    EXPECT_EQ(names[0], "Timing");
    EXPECT_EQ(names[1], "Logging");
    EXPECT_EQ(names[2], "Error");
    EXPECT_EQ(names[3], "Status");

    std::vector<std::string> info;
    g_info = &info;
    brep::SetAspectTraceSinkForTest(&TraceInfo);
    QStringList reports;
    CommandContext ctx;
    ctx.ReportStatus = [&](const QString& message) { reports.push_back(message); };
    manager->run("test.ok", ctx);
    brep::SetAspectTraceSinkForTest(nullptr);
    g_info = nullptr;

    int runLogs = 0;
    int elapsed = 0;
    for (const std::string& line : info)
    {
        if (line.find("command.run") != std::string::npos &&
            line.find("elapsed_ms=") == std::string::npos)
        {
            ++runLogs;
        }
        if (line.find("elapsed_ms=") != std::string::npos &&
            line.find("command.run") != std::string::npos)
        {
            ++elapsed;
        }
    }
    EXPECT_EQ(runLogs, 1);
    EXPECT_EQ(elapsed, 1);
    ASSERT_EQ(reports.size(), 1);
}

}  // namespace
}  // namespace brep::viewer::commands
