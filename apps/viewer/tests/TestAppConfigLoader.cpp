#include "bootstrap/AppConfigLoader.h"

#include <gtest/gtest.h>

#if defined(_WIN32)
#include <stdlib.h>
#else
#include <cstdlib>
#endif

namespace brep::viewer::bootstrap
{
namespace
{

TEST(AppConfigLoader, ParseBooleanBackendValues)
{
  BooleanBackend backend{};
  EXPECT_TRUE(TryParseBooleanBackend("default", backend));
  EXPECT_EQ(backend, BooleanBackend::Default);
  EXPECT_TRUE(TryParseBooleanBackend("STUB", backend));
  EXPECT_EQ(backend, BooleanBackend::Stub);
  EXPECT_TRUE(TryParseBooleanBackend(" Occt ", backend));
  EXPECT_EQ(backend, BooleanBackend::Occt);
  EXPECT_FALSE(TryParseBooleanBackend("invalid", backend));
}

TEST(AppConfigLoader, ParsePluginModuleList)
{
  const auto paths = ParsePluginModuleList("a.dll;b.dll:c.dll");
  ASSERT_EQ(paths.size(), 3u);
  EXPECT_EQ(paths[0], "a.dll");
  EXPECT_EQ(paths[1], "b.dll");
  EXPECT_EQ(paths[2], "c.dll");
}

TEST(AppConfigLoader, CommandLineOverridesEnvironment)
{
  const char arg0[] = "brep_viewer";
  const char arg1[] = "--boolean-backend=stub";
  const char arg2[] = "--plugin=plugins/sample.dll";
  char* argv[] = {const_cast<char*>(arg0), const_cast<char*>(arg1),
                  const_cast<char*>(arg2)};

#ifdef _WIN32
  _putenv("BREP_BOOLEAN_BACKEND=default");
#else
  setenv("BREP_BOOLEAN_BACKEND", "default", 1);
#endif

  const AppConfig config = LoadAppConfig(3, argv);
  EXPECT_EQ(config.booleanBackend, BooleanBackend::Stub);
  ASSERT_EQ(config.pluginModulePaths.size(), 1u);
  EXPECT_EQ(config.pluginModulePaths.front(), "plugins/sample.dll");
}

}  // namespace
}  // namespace brep::viewer::bootstrap
