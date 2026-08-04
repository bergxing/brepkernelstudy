#pragma once

#include "brep/math.hpp"

#include <format>
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
void log_format(LogLevel level, std::format_string<Args...> fmt, Args&&... args) {
  log_message(level, std::format(fmt, std::forward<Args>(args)...));
}

}  // namespace brep

// std::formatter for Point3d / Vector3d (C++20 format used by log_format)
template <>
struct std::formatter<brep::Point3d> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  auto format(const brep::Point3d& p, format_context& ctx) const {
    return std::format_to(ctx.out(), "({:.6g}, {:.6g}, {:.6g})", p.x(), p.y(),
                          p.z());
  }
};

template <>
struct std::formatter<brep::Vector3d> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  auto format(const brep::Vector3d& v, format_context& ctx) const {
    return std::format_to(ctx.out(), "({:.6g}, {:.6g}, {:.6g})", v.x(), v.y(),
                          v.z());
  }
};

#define BREP_TRACE(...) ::brep::log_format(::brep::LogLevel::Trace, __VA_ARGS__)
#define BREP_DEBUG(...) ::brep::log_format(::brep::LogLevel::Debug, __VA_ARGS__)
#define BREP_INFO(...) ::brep::log_format(::brep::LogLevel::Info, __VA_ARGS__)
#define BREP_WARN(...) ::brep::log_format(::brep::LogLevel::Warn, __VA_ARGS__)
#define BREP_ERROR(...) ::brep::log_format(::brep::LogLevel::Error, __VA_ARGS__)
