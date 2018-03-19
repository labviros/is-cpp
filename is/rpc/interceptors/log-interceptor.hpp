#pragma once

#include "../../core.hpp"
#include "../interceptor.hpp"
#include "../service-provider.hpp"

namespace is {

class LogInterceptor : public Interceptor {
  pb::Timestamp started_at;

 public:
  void before_call(Context* context) override;
  void after_call(Context* context) override;
};  // LogInterceptor

}  // namespace is