#pragma once

#include "bootstrap/AppConfig.h"

#include <memory>

namespace Hypodermic
{
class Container;
}

namespace brep::viewer
{

/// Process-level holder for IoC container (no Hypodermic includes here).
struct ApplicationContext
{
  std::shared_ptr<Hypodermic::Container> container;
  bootstrap::AppConfig config;
};

}  // namespace brep::viewer
