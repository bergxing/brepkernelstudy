#include "bootstrap/ViewerAdapterServices.h"

#include "adapter/DocumentService.h"
#include "adapter/ISceneServiceFactory.h"

#include <Hypodermic/Container.h>

namespace brep::viewer::bootstrap
{

adapter::IDocumentService& ResolveDocumentService(
    const std::shared_ptr<Hypodermic::Container>& container)
{
  if (!container)
  {
    static adapter::DocumentService fallback{};
    return fallback;
  }
  return *container->resolve<adapter::IDocumentService>();
}

adapter::ISceneServiceFactory& ResolveSceneServiceFactory(
    const std::shared_ptr<Hypodermic::Container>& container)
{
  if (!container)
  {
    static adapter::DefaultSceneServiceFactory fallback{};
    return fallback;
  }
  return *container->resolve<adapter::ISceneServiceFactory>();
}

}  // namespace brep::viewer::bootstrap
