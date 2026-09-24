#include "brep/Log.h"

// Keep spdlog confined to this translation unit (header-only).
#define SPDLOG_HEADER_ONLY
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

#include <memory>
#include <string>
#include <vector>

namespace brep
{
namespace
{

std::shared_ptr<spdlog::logger>& logger_storage()
{
  static std::shared_ptr<spdlog::logger> log;
  return log;
}

spdlog::level::level_enum to_spdlog(LogLevel level)
{
  switch (level)
{
    case LogLevel::Trace:
      return spdlog::level::trace;
    case LogLevel::Debug:
      return spdlog::level::debug;
    case LogLevel::Info:
      return spdlog::level::info;
    case LogLevel::Warn:
      return spdlog::level::warn;
    case LogLevel::Error:
      return spdlog::level::err;
  }
  return spdlog::level::info;
}

}  // namespace

void InitLogging(std::string_view log_file, LogLevel level, bool force)
{
  auto& log = logger_storage();
  if (log && !force)
  {
    return;
  }

  std::vector<spdlog::sink_ptr> sinks;
  sinks.push_back(std::make_shared<spdlog::sinks::stdout_sink_st>());
  if (!log_file.empty())
  {
    sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_st>(
        std::string{log_file}, true));
  }

  log = std::make_shared<spdlog::logger>("brep", sinks.begin(), sinks.end());
  log->set_level(to_spdlog(level));
  log->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
  log->flush_on(spdlog::level::info);
}

void LogMessage(LogLevel level, std::string_view message)
{
  auto& log = logger_storage();
  if (!log)
  {
    InitLogging();
  }
  switch (level)
  {
    case LogLevel::Trace:
      log->trace("{}", message);
      break;
    case LogLevel::Debug:
      log->debug("{}", message);
      break;
    case LogLevel::Info:
      log->info("{}", message);
      break;
    case LogLevel::Warn:
      log->warn("{}", message);
      break;
    case LogLevel::Error:
      log->error("{}", message);
      break;
  }
}

}  // namespace brep
