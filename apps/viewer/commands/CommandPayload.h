#pragma once

#include "commands/CommandTypes.h"

#include <QByteArray>

#include <functional>

namespace brep::viewer::commands
{

/// Qt data for command.* aspect events. Payload is this type.
struct CommandPayload
{
    CommandResult ResultStorage{};
    const CommandResult* Result{nullptr};
    QString Prompt;
    const std::function<void(const QString&)>* ReportStatus{nullptr};
    bool AnnounceCancel{false};
    QByteArray DetailUtf8;
};

}  // namespace brep::viewer::commands
