#include "bootstrap/ViewerRuntimeModule.h"

#include "commands/CommandRegistry.h"

#include <Hypodermic/ComponentContext.h>
#include <Hypodermic/ContainerBuilder.h>
#include <Hypodermic/SingleInstance.h>

namespace brep::viewer::bootstrap
{

void ViewerRuntimeModule::RegisterServices(Hypodermic::ContainerBuilder& builder)
{
  // Application Singleton — builtin command factories registered once.
  builder.registerInstanceFactory(
      [](Hypodermic::ComponentContext&) -> std::shared_ptr<commands::CommandRegistry> {
        auto registry = std::make_shared<commands::CommandRegistry>();
        commands::register_builtin_commands(*registry);
        return registry;
      })
      .singleInstance();
}

}  // namespace brep::viewer::bootstrap
