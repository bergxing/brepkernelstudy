#include "bootstrap/ModuleRegistrar.h"

namespace brep::viewer::bootstrap
{

void ModuleRegistrar::RegisterModule(std::unique_ptr<IModule> module)
{
  if (module)
  {
    m_modules.push_back(std::move(module));
  }
}

std::vector<std::unique_ptr<IModule>> ModuleRegistrar::TakeModules()
{
  return std::move(m_modules);
}

}  // namespace brep::viewer::bootstrap
