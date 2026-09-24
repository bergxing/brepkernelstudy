#pragma once

#include "Document.h"
#include "ecs/World.h"

namespace brep::viewer::adapter
{
class IDocumentService;
class ISceneService;
class ISceneServiceFactory;
}

namespace brep::viewer::commands
{

class DocumentHistory;
struct CommandContext;
struct SnapSession;
struct SnapSettings;

/// Holds stable pointers for filling CommandContext (Phase 2 IoC).
class CommandContextFactory
{
 public:
  void SetWorld(ecs::World* world) noexcept
  {
    m_world = world;
  }
  void SetSession(DocumentSession* session) noexcept
  {
    m_session = session;
  }
  void SetHistory(DocumentHistory* history) noexcept
  {
    m_history = history;
  }
  void SetSnapSettings(SnapSettings* settings) noexcept
  {
    m_snapSettings = settings;
  }
  void SetSnapSession(SnapSession* session) noexcept
  {
    m_snapSession = session;
  }
  void SetDocumentService(adapter::IDocumentService* service) noexcept
  {
    m_documentService = service;
  }
  void SetSceneService(adapter::ISceneService* scene) noexcept
  {
    m_sceneService = scene;
  }
  void SetSceneFactory(adapter::ISceneServiceFactory* factory) noexcept
  {
    m_sceneFactory = factory;
  }

  void PopulateBase(CommandContext& ctx) const noexcept;

 private:
  ecs::World* m_world{nullptr};
  DocumentSession* m_session{nullptr};
  DocumentHistory* m_history{nullptr};
  SnapSettings* m_snapSettings{nullptr};
  SnapSession* m_snapSession{nullptr};
  adapter::IDocumentService* m_documentService{nullptr};
  adapter::ISceneService* m_sceneService{nullptr};
  adapter::ISceneServiceFactory* m_sceneFactory{nullptr};
};

}  // namespace brep::viewer::commands
