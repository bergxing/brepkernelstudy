#include "bootstrap/ViewerUiModule.h"

#include <Hypodermic/ContainerBuilder.h>

namespace brep::viewer::bootstrap
{

void ViewerUiModule::RegisterServices(Hypodermic::ContainerBuilder& /*builder*/)
{
  // Phase 2: UI-scoped services resolved at MainWindow construction.
}

}  // namespace brep::viewer::bootstrap
