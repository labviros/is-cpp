#pragma once

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

namespace is {

inline std::shared_ptr<spdlog::logger> logger() {
  static auto ptr = [] {
    auto ptr = spdlog::stdout_color_mt("is");
    ptr->set_pattern("[%L][%t][%d-%m-%Y %H:%M:%S:%e] %v");
    return ptr;
  }();
  return ptr;
}

template <class... Args>
inline void info(Args&&... args) {
  logger()->info(args...);
}

template <class... Args>
inline void warn(Args&&... args) {
  logger()->warn(args...);
}

template <class... Args>
inline void error(Args&&... args) {
  logger()->error(args...);
}

template <class... Args>
inline void critical(Args&&... args) {
  logger()->critical(args...);
  std::exit(-1);
}

inline int set_loglevel(char level) {
  if (level == 'i')
    spdlog::set_level(spdlog::level::info);
  else if (level == 'w')
    spdlog::set_level(spdlog::level::warn);
  else if (level == 'e')
    spdlog::set_level(spdlog::level::err);
  else
    return -1;  // failed
  return 0;     // success
}

}  // namespace is