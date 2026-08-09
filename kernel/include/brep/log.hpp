#pragma once

#include "brep/math.hpp"

// Prefer fmt (bundled with spdlog) over std::format so MinGW/GCC 11 (Qt kit) works.
#include <spdlog/fmt/fmt.h>

#include <string_view>
#include <utility>

namespace brep {

enum class LogLevel { Trace, Debug, Info, Warn, Error };

/// Initialize logging (console + optional file). Safe to call repeatedly.
void init_logging(std::string_view log_file = {},
                  LogLevel level = LogLevel::Info,
                  bool force = false);

void log_message(LogLevel level, std::string_view message);

template <class... Args>
void log_format(LogLevel level, fmt::format_string<Args...> fmt, Args&&... args) {
  log_message(level, fmt::format(fmt, std::forward<Args>(args)...));
}

}  // namespace brep

template <>
struct fmt::formatter<brep::Point3d> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  auto format(const brep::Point3d& p, format_context& ctx) const {
    return fmt::format_to(ctx.out(), "({:.6g}, {:.6g}, {:.6g})", p.x(), p.y(),
                          p.z());
  }
};

template <>
struct fmt::formatter<brep::Vector3d> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  auto format(const brep::Vector3d& v, format_context& ctx) const {
    return fmt::format_to(ctx.out(), "({:.6g}, {:.6g}, {:.6g})", v.x(), v.y(),
                          v.z());
  }
};

#define BREP_TRACE(...) ::brep::log_format(::brep::LogLevel::Trace, __VA_ARGS__)
#define BREP_DEBUG(...) ::brep::log_format(::brep::LogLevel::Debug, __VA_ARGS__)
#define BREP_INFO(...) ::brep::log_format(::brep::LogLevel::Info, __VA_ARGS__)
#define BREP_WARN(...) ::brep::log_format(::brep::LogLevel::Warn, __VA_ARGS__)
#define BREP_ERROR(...) ::brep::log_format(::brep::LogLevel::Error, __VA_ARGS__)
