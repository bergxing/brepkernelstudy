#include "bootstrap/ModulePluginLoader.h"

#include "bootstrap/ModulePluginApi.h"

#include "api/Core.h"

#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace brep::viewer::bootstrap
{
namespace
{

struct LoadedLibrary
{
  std::string path;
#ifdef _WIN32
  HMODULE handle{nullptr};
#else
  void* handle{nullptr};
#endif
};

std::vector<LoadedLibrary>& loaded_libraries()
{
  static std::vector<LoadedLibrary> libraries;
  return libraries;
}

#ifdef _WIN32
void* load_library(const std::string& path)
{
  return static_cast<void*>(LoadLibraryA(path.c_str()));
}

void* resolve_symbol(void* handle, const char* name)
{
  if (!handle)
  {
    return nullptr;
  }
  return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle),
                                                name));
}

std::string last_load_error()
{
  const DWORD err = GetLastError();
  if (err == 0)
  {
    return {};
  }
  return "Win32 error " + std::to_string(err);
}
#else
void* load_library(const std::string& path)
{
  return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
}

void* resolve_symbol(void* handle, const char* name)
{
  if (!handle)
  {
    return nullptr;
  }
  return dlsym(handle, name);
}

std::string last_load_error()
{
  const char* err = dlerror();
  return err != nullptr ? std::string(err) : std::string{};
}
#endif

void load_plugin_module(const std::string& path, ModuleRegistrar& registrar)
{
  void* handle = load_library(path);
  if (!handle)
  {
    BREP_WARN("IoC plugin: failed to load '{}': {}", path, last_load_error());
    return;
  }

  auto* entry = reinterpret_cast<BrepRegisterModuleFn>(
      resolve_symbol(handle, kBrepRegisterModuleSymbol));
  if (!entry)
  {
    BREP_WARN("IoC plugin: '{}' missing symbol {}", path,
              kBrepRegisterModuleSymbol);
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
    return;
  }

  entry(registrar);
#ifdef _WIN32
  loaded_libraries().push_back(
      LoadedLibrary{.path = path, .handle = static_cast<HMODULE>(handle)});
#else
  loaded_libraries().push_back(LoadedLibrary{.path = path, .handle = handle});
#endif
  BREP_INFO("IoC plugin loaded: {}", path);
}

}  // namespace

void ModulePluginLoader::Load(const AppConfig& config,
                             ModuleRegistrar& registrar)
{
  for (const auto& path : config.pluginModulePaths)
  {
    if (path.empty())
    {
      continue;
    }
    load_plugin_module(path, registrar);
  }
}

}  // namespace brep::viewer::bootstrap
