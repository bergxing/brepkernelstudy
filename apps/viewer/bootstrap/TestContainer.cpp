#include "bootstrap/TestContainer.h"

#include "bootstrap/KernelServicesModule.h"

#include <Hypodermic/ContainerBuilder.h>

namespace brep::viewer::bootstrap
{

std::shared_ptr<Hypodermic::Container> BuildTestContainer(
    TestContainerOverrides overrides)
{
  Hypodermic::ContainerBuilder builder;
  KernelServicesModule{}.RegisterServices(builder);
  if (overrides)
  {
    overrides(builder);
  }
  return builder.build();
}

}  // namespace brep::viewer::bootstrap
