#pragma once

#include "Document.h"
#include "ecs/World.h"

#include "api/Mesh.h"

#include <QString>
#include <QWidget>

#include <functional>
#include <string>

namespace brep::viewer
{
class Camera;
class VulkanWindow;
}  // namespace brep::viewer

namespace brep::viewer::commands
{

class DocumentHistory;
struct SnapSession;
struct SnapSettings;

enum class CommandStatus
{
    Ok,
    Cancelled,
    Failed,
};

enum class CommandKind
{
    Instant,
    Interactive,
    View,
};

struct CommandResult
{
    CommandStatus Status{CommandStatus::Ok};
    QString Message;

    [[nodiscard]] static CommandResult Ok(QString msg = {})
    {
        return {CommandStatus::Ok, std::move(msg)};
    }

    [[nodiscard]] static CommandResult Cancelled(QString msg = {})
    {
        return {CommandStatus::Cancelled, std::move(msg)};
    }

    [[nodiscard]] static CommandResult Failed(QString msg)
    {
        return {CommandStatus::Failed, std::move(msg)};
    }

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return Status == CommandStatus::Ok;
    }
};

struct CommandContext
{
    ecs::World* World{nullptr};
    DocumentSession* Session{nullptr};
    DocumentHistory* History{nullptr};
    QWidget* ParentWidget{nullptr};
    Camera* ViewCamera{nullptr};
    VulkanWindow* Viewport{nullptr};
    SnapSettings* SnapSettingsRef{nullptr};
    SnapSession* SnapSessionRef{nullptr};
    std::string WoodAlbedoPath;
    int ViewportWidth{1};
    int ViewportHeight{1};

    std::function<void(const QString&)> ReportStatus;
    std::function<void()> RequestRedraw;
    std::function<void()> AfterDocumentReset;
    std::function<void()> RefreshUi;
    std::function<void(EdgeMesh)> SetPreviewEdges;
    /// Wire + optional translucent solid fill for interactive tool previews.
    std::function<void(EdgeMesh, TriangleMesh)> SetPreview;
    std::function<void()> ClearPreview;
    std::function<void(EdgeMesh)> SetSnapOverlay;
    std::function<void()> ClearSnapOverlay;
    std::function<void()> RefreshCursorTip;
};

}  // namespace brep::viewer::commands
