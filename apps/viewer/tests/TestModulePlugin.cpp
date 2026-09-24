#include "bootstrap/ApplicationContainer.h"
#include "bootstrap/ModulePluginApi.h"

#include <gtest/gtest.h>

namespace brep::viewer::bootstrap
{
namespace
{

TEST(ModulePlugin, MissingPluginPathIsIgnored)
{
  AppConfig config;
  config.pluginModulePaths = {"Z:/no/such/brep_viewer_plugin.dll"};
  EXPECT_NO_THROW({
    const auto container = BuildApplicationContainer(config);
    EXPECT_NE(container, nullptr);
  });
}

#if defined(BREP_HAS_VIEWER_PLUGIN_SAMPLE)
TEST(ModulePlugin, LoadsSamplePlugin)
{
#if defined(_WIN32)
  const char* plugin_path = "plugins/brep_viewer_plugin_sample.dll";
#else
  const char* plugin_path = "plugins/brep_viewer_plugin_sample.so";
#endif
  AppConfig config;
  config.pluginModulePaths = {plugin_path};
  const auto container = BuildApplicationContainer(config);
  EXPECT_NE(container, nullptr);
}
#endif

}  // namespace
}  // namespace brep::viewer::bootstrap
