#pragma once

#include "bootstrap/IModule.h"
#include "bootstrap/ModulePluginApi.h"

#include <memory>
#include <vector>

namespace brep::viewer::bootstrap
{

/// Collects modules registered by static and dynamic plugin entry points.
class ModuleRegistrar final : public IModuleRegistrar
{
 public:
  void RegisterModule(std::unique_ptr<IModule> module) override;

  [[nodiscard]] std::vector<std::unique_ptr<IModule>> TakeModules();

 private:
  std::vector<std::unique_ptr<IModule>> m_modules;
};

}  // namespace brep::viewer::bootstrap
