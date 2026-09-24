#include "commands/StatusAspect.h"

#include "commands/CommandPayload.h"

namespace brep::viewer::commands
{
namespace
{

const CommandPayload* PayloadOf(const brep::AspectEvent& event)
{
    if (!event.Site.starts_with("command.") || event.Payload == nullptr)
    {
        return nullptr;
    }
    return static_cast<const CommandPayload*>(event.Payload);
}

void Report(const CommandPayload& payload, const QString& message)
{
    if (payload.ReportStatus == nullptr || !(*payload.ReportStatus) ||
        message.isEmpty())
    {
        return;
    }
    (*payload.ReportStatus)(message);
}

}  // namespace

std::string_view StatusAspect::Name() const noexcept
{
    return "Status";
}

void StatusAspect::Before(const brep::AspectEvent&) {}

void StatusAspect::After(const brep::AspectEvent& event)
{
    const CommandPayload* payload = PayloadOf(event);
    if (payload == nullptr)
    {
        return;
    }
    if (event.Site == "command.tool.start")
    {
        Report(*payload, payload->Prompt);
        return;
    }
    if (event.Site == "command.run" || event.Site == "command.tool.finish")
    {
        if (payload->Result != nullptr)
        {
            Report(*payload, payload->Result->Message);
        }
        return;
    }
    if (event.Site == "command.tool.cancel" && payload->AnnounceCancel)
    {
        Report(*payload, QStringLiteral("已取消: %1")
                             .arg(QString::fromUtf8(event.Subject.data(),
                                                    static_cast<qsizetype>(
                                                        event.Subject.size()))));
    }
}

void StatusAspect::OnError(const brep::AspectEvent& event,
                           const std::exception& error)
{
    const CommandPayload* payload = PayloadOf(event);
    if (payload == nullptr)
    {
        return;
    }
    Report(*payload, QString::fromUtf8(error.what()));
}

}  // namespace brep::viewer::commands
