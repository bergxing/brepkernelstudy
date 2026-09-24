#pragma once

#include "adapter/ISceneService.h"

#include <memory>

namespace brep
{
class Document;
}

namespace Hypodermic
{
class Container;
}

namespace brep::viewer::bootstrap
{

/// Per-Document nested IoC scope: child Container + ISceneService (1:1 with Document).
class DocumentScope
{
 public:
  [[nodiscard]] static std::unique_ptr<DocumentScope> Create(
      const std::shared_ptr<Hypodermic::Container>& appContainer,
      brep::Document* document);

  /// Rebuild nested container when World adopts or replaces Document.
  void Rebind(brep::Document* document);

  [[nodiscard]] brep::Document* Document() const noexcept
  {
      return m_document; 
  }
  [[nodiscard]] adapter::ISceneService& scene() noexcept
  {
      return *m_scene; 
  }
  [[nodiscard]] const adapter::ISceneService& scene() const noexcept
  {
      return *m_scene; 
  }

 private:
  DocumentScope(std::shared_ptr<Hypodermic::Container> appContainer,
                brep::Document* document);

  void build_for_document(brep::Document* document);

  std::shared_ptr<Hypodermic::Container> m_appContainer;
  std::shared_ptr<Hypodermic::Container> m_documentContainer;
  std::shared_ptr<adapter::ISceneService> m_scene;
  brep::Document* m_document{nullptr};
};

}  // namespace brep::viewer::bootstrap
