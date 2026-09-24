#include "bootstrap/ViewerAdapterModule.h"

#include "adapter/DocumentService.h"
#include "adapter/ISceneServiceFactory.h"

#include <Hypodermic/ContainerBuilder.h>
#include <Hypodermic/SingleInstance.h>

namespace brep::viewer::bootstrap
{

void ViewerAdapterModule::RegisterServices(
    Hypodermic::ContainerBuilder& builder)
{
  builder.registerType<adapter::DocumentService>()
      .as<adapter::IDocumentService>()
      .singleInstance();

  builder.registerType<adapter::DefaultSceneServiceFactory>()
      .as<adapter::ISceneServiceFactory>()
      .singleInstance();
}

}  // namespace brep::viewer::bootstrap
