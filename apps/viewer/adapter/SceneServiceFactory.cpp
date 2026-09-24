#include "adapter/ISceneServiceFactory.h"

namespace brep::viewer::adapter
{

std::unique_ptr<ISceneService> DefaultSceneServiceFactory::Create(
    brep::Document* doc)
{
  return std::make_unique<SceneAdapter>(doc);
}

}  // namespace brep::viewer::adapter
