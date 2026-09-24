#include "bootstrap/IModule.h"
#include "bootstrap/ModulePluginApi.h"

#include "api/Core.h"

namespace brep::viewer::plugins
{
namespace
{

/// Sample dynamic IoC module (Phase 5 smoke). Order 15 — between adapter and runtime.
class SamplePluginModule final : public bootstrap::IModule
{
 public:
  [[nodiscard]] int Order() const override
  {
    return 15;
  }

  void RegisterServices(Hypodermic::ContainerBuilder& /*builder*/) override
  {
    BREP_INFO("SamplePluginModule registered via brep_register_module");
  }
};

}  // namespace
}  // namespace brep::viewer::plugins

extern "C" BREP_PLUGIN_API void brep_register_module(
    brep::viewer::bootstrap::IModuleRegistrar& registrar)
{
  registrar.RegisterModule(
      std::make_unique<brep::viewer::plugins::SamplePluginModule>());
}
