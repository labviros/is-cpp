#include "log-interceptor.hpp"

namespace is {

void LogInterceptor::before_call(Context* context) {
  started_at = current_time();
}

void LogInterceptor::after_call(Context* context) {
  auto took = pb::TimeUtil::DurationToMilliseconds(current_time() - started_at);
  auto code = common::StatusCode_Name(context->status().code());
  auto service = context->topic();

  if (context->status().code() == StatusCode::OK) {
    info("{};{}ms;{}", service, took, code);
  } else {
    warn("{};{}ms;{};'{}'", service, took, code, context->status().why());
  }
}

}  // namespace is