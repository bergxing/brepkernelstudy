#pragma once

#include "bootstrap/AppConfig.h"
#include "bootstrap/IModule.h"

namespace brep::viewer::bootstrap
{

/// Registers kernel-facing services (IBooleanEvaluator, …). Scope: Application.
class KernelServicesModule final : public IModule
{
 public:
  explicit KernelServicesModule(AppConfig config = {});

  [[nodiscard]] int Order() const override
  {
    return 10;
  }
  void RegisterServices(Hypodermic::ContainerBuilder& builder) override;

 private:
  AppConfig m_config;
};

}  // namespace brep::viewer::bootstrap
