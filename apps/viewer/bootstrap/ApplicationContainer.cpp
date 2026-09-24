#include "bootstrap/ApplicationContainer.h"

#include "bootstrap/KernelServicesModule.h"
#include "bootstrap/ModulePluginLoader.h"
#include "bootstrap/ModuleRegistrar.h"
#include "bootstrap/ViewerAdapterModule.h"
#include "bootstrap/ViewerRuntimeModule.h"
#include "bootstrap/ViewerUiModule.h"

#include "api/Core.h"

#include <Hypodermic/ContainerBuilder.h>

#include <algorithm>
#include <memory>
#include <vector>

namespace brep::viewer::bootstrap
{
namespace
{

void register_modules(Hypodermic::ContainerBuilder& builder,
                      const std::vector<std::unique_ptr<IModule>>& modules)
{
  for (const auto& module : modules)
  {
    module->RegisterServices(builder);
  }
}

}  // namespace

std::shared_ptr<Hypodermic::Container> BuildApplicationContainer(
    const AppConfig& config)
{
  ModuleRegistrar pluginRegistrar;
  ModulePluginLoader pluginLoader;
  pluginLoader.Load(config, pluginRegistrar);

  std::vector<std::unique_ptr<IModule>> modules;
  modules.push_back(std::make_unique<KernelServicesModule>(config));

  for (auto& pluginModule : pluginRegistrar.TakeModules())
  {
    modules.push_back(std::move(pluginModule));
  }

  modules.push_back(std::make_unique<ViewerAdapterModule>());
  modules.push_back(std::make_unique<ViewerRuntimeModule>());
  modules.push_back(std::make_unique<ViewerUiModule>());

  std::sort(modules.begin(), modules.end(),
            [](const std::unique_ptr<IModule>& a,
               const std::unique_ptr<IModule>& b) {
              return a->Order() < b->Order();
            });

  Hypodermic::ContainerBuilder builder;
  register_modules(builder, modules);

  BREP_DEBUG("IoC: building application container ({} modules)",
             modules.size());
  std::shared_ptr<Hypodermic::Container> container = builder.build();
  brep::InstallProcessAspects();
  return container;
}

}  // namespace brep::viewer::bootstrap
