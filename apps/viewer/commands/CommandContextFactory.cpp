#include "commands/CommandContextFactory.h"

#include "commands/CommandTypes.h"
#include "commands/DocumentHistory.h"
#include "commands/snap/SnapSettings.h"

namespace brep::viewer::commands
{

void CommandContextFactory::PopulateBase(CommandContext& ctx) const noexcept
{
  ctx.World = m_world;
  ctx.Session = m_session;
  ctx.History = m_history;
  ctx.DocumentService = m_documentService;
  ctx.Scene = m_sceneService;
  ctx.SceneFactory = m_sceneFactory;
  ctx.SnapSettingsRef = m_snapSettings;
  ctx.SnapSessionRef = m_snapSession;
}

}  // namespace brep::viewer::commands
