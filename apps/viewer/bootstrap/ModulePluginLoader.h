#pragma once

#include "bootstrap/AppConfig.h"
#include "bootstrap/ModuleRegistrar.h"

namespace brep::viewer::bootstrap
{

/// Loads plugin DLLs listed in AppConfig and registers their IModule instances.
class ModulePluginLoader
{
 public:
  void Load(const AppConfig& config, ModuleRegistrar& registrar);
};

}  // namespace brep::viewer::bootstrap
