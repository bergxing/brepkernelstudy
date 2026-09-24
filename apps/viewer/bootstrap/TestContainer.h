#pragma once

#include <functional>
#include <memory>

namespace Hypodermic
{
class Container;
class ContainerBuilder;
}

namespace brep::viewer::bootstrap
{

using TestContainerOverrides =
    std::function<void(Hypodermic::ContainerBuilder&)>;

[[nodiscard]] std::shared_ptr<Hypodermic::Container> BuildTestContainer(
    TestContainerOverrides overrides = {});

}  // namespace brep::viewer::bootstrap
