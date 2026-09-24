#pragma once

#include "commands/CommandManager.h"
#include "commands/CommandRegistry.h"

#include <memory>

namespace Hypodermic
{
class Container;
}

namespace brep::viewer::bootstrap
{

/// Application-scoped command registry (ViewerRuntimeModule).
[[nodiscard]] commands::CommandRegistry& ResolveCommandRegistry(
    const std::shared_ptr<Hypodermic::Container>& container);

/// Per-window CommandManager (DocumentHistory is not shared).
[[nodiscard]] std::unique_ptr<commands::CommandManager> CreateCommandManager(
    const std::shared_ptr<Hypodermic::Container>& container);

}  // namespace brep::viewer::bootstrap
