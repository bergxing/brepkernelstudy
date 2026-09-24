#include "bootstrap/DocumentScope.h"

#include "bootstrap/DocumentScopeModule.h"

#include "api/Core.h"

#include <Hypodermic/Container.h>
#include <Hypodermic/ContainerBuilder.h>

namespace brep::viewer::bootstrap
{

DocumentScope::DocumentScope(
    std::shared_ptr<Hypodermic::Container> appContainer,
    brep::Document* document)
    : m_appContainer(std::move(appContainer))
{
  build_for_document(document);
}

std::unique_ptr<DocumentScope> DocumentScope::Create(
    const std::shared_ptr<Hypodermic::Container>& appContainer,
    brep::Document* document)
{
  if (!appContainer || !document)
  {
    return nullptr;
  }
  return std::unique_ptr<DocumentScope>(
      new DocumentScope(appContainer, document));
}

void DocumentScope::Rebind(brep::Document* document)
{
  if (!document || document == m_document)
  {
    return;
  }
  build_for_document(document);
}

void DocumentScope::build_for_document(brep::Document* document)
{
  m_document = document;

  Hypodermic::ContainerBuilder builder;
  DocumentScopeModule::Register(builder, document);
  m_documentContainer = builder.buildNestedContainerFrom(*m_appContainer);
  m_scene = m_documentContainer->resolve<adapter::ISceneService>();

  BREP_DEBUG("IoC: document scope bound to '{}'", document->Name);
}

}  // namespace brep::viewer::bootstrap
