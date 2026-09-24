#pragma once

#include "bootstrap/IModule.h"

#include <memory>

namespace brep::viewer::bootstrap
{

/// Host-side sink for plugin DLL entry points (Phase 5).
class IModuleRegistrar
{
 public:
  virtual ~IModuleRegistrar() = default;

  virtual void RegisterModule(std::unique_ptr<IModule> module) = 0;
};

}  // namespace brep::viewer::bootstrap

inline constexpr char kBrepRegisterModuleSymbol[] = "brep_register_module";

/// C ABI for dynamically loaded viewer IoC plugin modules.
extern "C"
{

using BrepRegisterModuleFn = void (*)(
    brep::viewer::bootstrap::IModuleRegistrar& registrar);

}  // extern "C"

#if defined(_WIN32)
#if defined(BREP_PLUGIN_EXPORTS)
#define BREP_PLUGIN_API __declspec(dllexport)
#else
#define BREP_PLUGIN_API __declspec(dllimport)
#endif
#else
#define BREP_PLUGIN_API __attribute__((visibility("default")))
#endif
