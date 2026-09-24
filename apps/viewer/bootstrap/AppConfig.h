#pragma once

#include "bootstrap/BooleanBackend.h"

#include <string>
#include <vector>

namespace brep::viewer::bootstrap
{

/// Process-level viewer configuration for Composition Root (Phase 0+).
///
/// Environment (optional, overridden by CLI):
///   BREP_LOG_PATH          — log file path (default brep_viewer.log)
///   BREP_BOOLEAN_BACKEND   — default | stub | occt
///   BREP_PLUGIN_MODULES    — plugin paths separated by ';' or ':'
///
/// CLI (optional):
///   --boolean-backend=<default|stub|occt>
///   --plugin=<path>        — repeatable
///   --log-path=<path>
struct AppConfig
{
  std::string logPath{"brep_viewer.log"};
  BooleanBackend booleanBackend{BooleanBackend::Default};
  /// Optional plugin module paths (.dll / .so) exporting `brep_register_module`.
  std::vector<std::string> pluginModulePaths;
};

}  // namespace brep::viewer::bootstrap
