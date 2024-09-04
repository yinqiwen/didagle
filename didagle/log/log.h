// Copyright (c) 2021, Tencent Inc.
// All rights reserved.
// Created on 2021/04/19
// Authors: qiyingwang (qiyingwang@tencent.com)
#pragma once

#include <stdio.h>
#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "spdlog/logger.h"

#include "fmt/ostream.h"  // do NOT put this line before spdlog

namespace didagle {

class Logger {
 private:
 public:
  virtual bool ShouldLog(spdlog::level::level_enum log_level) = 0;
  virtual void Log(spdlog::source_loc loc, spdlog::level::level_enum lvl, std::string_view msg) = 0;
  static Logger& GetDidagleLogger();
  static void SetLogger(std::shared_ptr<Logger> logger);
  virtual ~Logger() {}
};

class Spdlogger : public Logger {
 public:
  Spdlogger();
  virtual bool ShouldLog(spdlog::level::level_enum log_level);
  virtual void Log(spdlog::source_loc loc, spdlog::level::level_enum lvl, std::string_view msg);
};

}  // namespace didagle

#define DIDAGLE_LOG(level, ...)                                                                                   \
  do {                                                                                                            \
    if (didagle::Logger::GetDidagleLogger().ShouldLog(level)) {                                                   \
      std::string s = fmt::format(__VA_ARGS__);                                                                   \
      didagle::Logger::GetDidagleLogger().Log(spdlog::source_loc{__FILE__, __LINE__, SPDLOG_FUNCTION}, level, s); \
    }                                                                                                             \
  } while (0)

#define DIDAGLE_ERROR(...) DIDAGLE_LOG(spdlog::level::err, __VA_ARGS__)

#define DIDAGLE_WARN(...) DIDAGLE_LOG(spdlog::level::warn, __VA_ARGS__)

#define DIDAGLE_INFO(...) DIDAGLE_LOG(spdlog::level::info, __VA_ARGS__)

#define DIDAGLE_DEBUG(...) DIDAGLE_LOG(spdlog::level::debug, __VA_ARGS__)

#define DIDAGLE_LOG_EVERY_N(level, n, ...)                                \
  do {                                                                    \
    static std::atomic<int> LOG_OCCURRENCES(0), LOG_OCCURRENCES_MOD_N(0); \
    ++LOG_OCCURRENCES;                                                    \
    if (++LOG_OCCURRENCES_MOD_N > n) LOG_OCCURRENCES_MOD_N -= n;          \
    if (LOG_OCCURRENCES_MOD_N == 1) {                                     \
      DIDAGLE_LOG(level, __VA_ARGS__);                                    \
    }                                                                     \
  } while (0)

#define DIDAGLE_WARN_EVERY_N(N, ...) DIDAGLE_LOG_EVERY_N(spdlog::level::warn, N, __VA_ARGS__)
