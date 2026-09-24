#include "bootstrap/DocumentScopeModule.h"

#include "adapter/ISceneService.h"
#include "adapter/SceneAdapter.h"

#include <Hypodermic/ComponentContext.h>
#include <Hypodermic/SingleInstance.h>

namespace brep::viewer::bootstrap
{

void DocumentScopeModule::Register(Hypodermic::ContainerBuilder& builder,
                                   brep::Document* document)
{
  // Document scope: ISceneService 1:1 with Document (singleInstance within nested container).
  builder.registerInstanceFactory(
      [document](Hypodermic::ComponentContext&)
          -> std::shared_ptr<adapter::SceneAdapter> {
        return std::make_shared<adapter::SceneAdapter>(document);
      })
      .as<adapter::ISceneService>()
      .singleInstance();
}

}  // namespace brep::viewer::bootstrap
