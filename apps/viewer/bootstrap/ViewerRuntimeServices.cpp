#include "bootstrap/ViewerRuntimeServices.h"

#include "commands/StatusAspect.h"

#include "api/Base.h"

#include <Hypodermic/Container.h>

namespace brep::viewer::bootstrap
{

commands::CommandRegistry& ResolveCommandRegistry(
    const std::shared_ptr<Hypodermic::Container>& container)
{
  if (!container)
  {
    static commands::CommandRegistry fallback{};
    static bool seeded = false;
    if (!seeded)
    {
      commands::register_builtin_commands(fallback);
      seeded = true;
    }
    return fallback;
  }
  return *container->resolve<commands::CommandRegistry>();
}

std::unique_ptr<commands::CommandManager> CreateCommandManager(
    const std::shared_ptr<Hypodermic::Container>& container)
{
  brep::AspectChain chain;
  chain.Add(std::make_unique<brep::TimingAspect>());
  chain.Add(std::make_unique<brep::LoggingAspect>());
  chain.Add(std::make_unique<brep::ErrorAspect>());
  chain.Add(std::make_unique<commands::StatusAspect>());
  return std::make_unique<commands::CommandManager>(
      ResolveCommandRegistry(container), std::move(chain));
}

}  // namespace brep::viewer::bootstrap
