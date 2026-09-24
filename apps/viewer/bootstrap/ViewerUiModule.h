#pragma once

#include "bootstrap/IModule.h"

namespace brep::viewer::bootstrap
{

/// Phase 2: MainWindow dependency aggregation. Phase 0: empty registration.
class ViewerUiModule final : public IModule
{
 public:
  [[nodiscard]] int Order() const override
  {
    return 40;
  }
  void RegisterServices(Hypodermic::ContainerBuilder& builder) override;
};

}  // namespace brep::viewer::bootstrap
