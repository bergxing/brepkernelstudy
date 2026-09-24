#pragma once

#include "bootstrap/AppConfig.h"

#include <memory>
#include <vector>

namespace Hypodermic
{
class Container;
}

namespace brep::viewer::bootstrap
{

[[nodiscard]] std::shared_ptr<Hypodermic::Container> BuildApplicationContainer(
    const AppConfig& config = {});

}  // namespace brep::viewer::bootstrap
