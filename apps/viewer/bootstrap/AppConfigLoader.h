#pragma once

#include "bootstrap/AppConfig.h"

namespace brep::viewer::bootstrap
{

/// Load process config: defaults → environment → CLI (later wins).
[[nodiscard]] AppConfig LoadAppConfig(int argc, char* argv[]);

/// Parse `default` / `stub` / `occt` (case-insensitive). Returns false if unknown.
[[nodiscard]] bool TryParseBooleanBackend(std::string_view text,
                                           BooleanBackend& out);

/// Split `BREP_PLUGIN_MODULES` / `--plugin` list (`;` or `:` separators).
[[nodiscard]] std::vector<std::string> ParsePluginModuleList(
    std::string_view text);

}  // namespace brep::viewer::bootstrap
