#pragma once

#include "bootstrap/IModule.h"

namespace brep::viewer::bootstrap
{

/// Phase 2: CommandRegistry / builtin commands. Phase 0: empty registration.
class ViewerRuntimeModule final : public IModule
{
 public:
  [[nodiscard]] int Order() const override
  {
    return 30;
  }
  void RegisterServices(Hypodermic::ContainerBuilder& builder) override;
};

}  // namespace brep::viewer::bootstrap
