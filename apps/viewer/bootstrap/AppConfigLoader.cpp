#include "bootstrap/AppConfigLoader.h"

#include "api/Core.h"

#include <cctype>
#include <cstdlib>
#include <string>
#include <string_view>

namespace brep::viewer::bootstrap
{
namespace
{

constexpr std::string_view kEnvBooleanBackend = "BREP_BOOLEAN_BACKEND";
constexpr std::string_view kEnvPluginModules = "BREP_PLUGIN_MODULES";
constexpr std::string_view kEnvLogPath = "BREP_LOG_PATH";

[[nodiscard]] std::string_view trim(std::string_view value)
{
  while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
  {
    value.remove_prefix(1);
  }
  while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
  {
    value.remove_suffix(1);
  }
  return value;
}

[[nodiscard]] std::string to_lower_ascii(std::string_view value)
{
  std::string out;
  out.reserve(value.size());
  for (const char ch : trim(value))
  {
    out.push_back(static_cast<char>(
        std::tolower(static_cast<unsigned char>(ch))));
  }
  return out;
}

[[nodiscard]] bool parse_flag_value(char* arg, std::string_view flag,
                                    std::string_view& value_out)
{
  const std::string_view text(arg);
  if (!text.starts_with(flag))
  {
    return false;
  }
  if (text.size() == flag.size())
  {
    value_out = {};
    return true;
  }
  if (text[flag.size()] != '=')
  {
    return false;
  }
  value_out = trim(text.substr(flag.size() + 1));
  return true;
}

void apply_boolean_backend_env(AppConfig& config)
{
  const char* raw = std::getenv("BREP_BOOLEAN_BACKEND");
  if (raw == nullptr || raw[0] == '\0')
  {
    return;
  }
  BooleanBackend backend{};
  if (TryParseBooleanBackend(raw, backend))
  {
    config.booleanBackend = backend;
    return;
  }
  BREP_WARN("Ignoring invalid {}='{}'", kEnvBooleanBackend, raw);
}

void apply_plugin_modules_env(AppConfig& config)
{
  const char* raw = std::getenv("BREP_PLUGIN_MODULES");
  if (raw == nullptr || raw[0] == '\0')
  {
    return;
  }
  for (const auto& path : ParsePluginModuleList(raw))
  {
    config.pluginModulePaths.push_back(path);
  }
}

void apply_log_path_env(AppConfig& config)
{
  const char* raw = std::getenv("BREP_LOG_PATH");
  if (raw == nullptr || raw[0] == '\0')
  {
    return;
  }
  config.logPath = raw;
}

void apply_environment(AppConfig& config)
{
  apply_log_path_env(config);
  apply_boolean_backend_env(config);
  apply_plugin_modules_env(config);
}

void apply_command_line(int argc, char* argv[], AppConfig& config)
{
  for (int i = 1; i < argc; ++i)
  {
    const std::string_view arg = argv[i] != nullptr ? argv[i] : "";
    if (arg == "--help" || arg == "-h")
    {
      continue;
    }

    std::string_view value;
    if (parse_flag_value(argv[i], "--boolean-backend", value))
    {
      if (value.empty() && i + 1 < argc)
      {
        value = trim(argv[++i]);
      }
      BooleanBackend backend{};
      if (TryParseBooleanBackend(value, backend))
      {
        config.booleanBackend = backend;
      } else
      {
        BREP_WARN("Ignoring invalid --boolean-backend='{}'", value);
      }
      continue;
    }

    if (parse_flag_value(argv[i], "--plugin", value))
    {
      if (value.empty() && i + 1 < argc)
      {
        value = trim(argv[++i]);
      }
      if (!value.empty())
      {
        config.pluginModulePaths.emplace_back(value);
      }
      continue;
    }

    if (parse_flag_value(argv[i], "--log-path", value))
    {
      if (value.empty() && i + 1 < argc)
      {
        value = trim(argv[++i]);
      }
      if (!value.empty())
      {
        config.logPath = std::string(value);
      }
    }
  }
}

}  // namespace

bool TryParseBooleanBackend(std::string_view text, BooleanBackend& out)
{
  const std::string normalized = to_lower_ascii(text);
  if (normalized == "default")
  {
    out = BooleanBackend::Default;
    return true;
  }
  if (normalized == "stub")
  {
    out = BooleanBackend::Stub;
    return true;
  }
  if (normalized == "occt")
  {
    out = BooleanBackend::Occt;
    return true;
  }
  return false;
}

std::vector<std::string> ParsePluginModuleList(std::string_view text)
{
  std::vector<std::string> paths;
  std::string_view rest = trim(text);
  while (!rest.empty())
  {
    std::size_t split = rest.find_first_of(";:");
    const std::string_view token =
        trim(split == std::string_view::npos ? rest : rest.substr(0, split));
    if (!token.empty())
    {
      paths.emplace_back(token);
    }
    if (split == std::string_view::npos)
    {
      break;
    }
    rest = trim(rest.substr(split + 1));
  }
  return paths;
}

AppConfig LoadAppConfig(int argc, char* argv[])
{
  AppConfig config;
  apply_environment(config);
  if (argc > 0 && argv != nullptr)
  {
    apply_command_line(argc, argv, config);
  }
  return config;
}

}  // namespace brep::viewer::bootstrap
