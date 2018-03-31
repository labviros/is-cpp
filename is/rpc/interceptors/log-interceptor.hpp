#pragma once

#include <spdlog/spdlog.h>
#include "../../core.hpp"
#include "../interceptor.hpp"
#include "../service-provider.hpp"

namespace is {

class LogInterceptor : public InterceptorConcept {
  pb::Timestamp started_at;
  std::shared_ptr<spdlog::logger> logger;

 public:
  LogInterceptor(char level = 'i');
  InterceptorConcept* copy() const;
  void before_call(Context* context);
  void after_call(Context* context);
};  // LogInterceptor

}  // namespace is