#pragma once

#include "../../core.hpp"
#include "../interceptor.hpp"
#include "../service-provider.hpp"
#include <spdlog/spdlog.h>

namespace is {

class LogInterceptor : public Interceptor {
  pb::Timestamp started_at;
  std::shared_ptr<spdlog::logger> logger;
 public:
  LogInterceptor(char level = 'i');
  void before_call(Context* context) override;
  void after_call(Context* context) override;
};  // LogInterceptor

}  // namespace is