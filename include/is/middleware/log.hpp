#pragma once

#include "../log.hpp"
#include "../rpc.hpp"

namespace is {

class RPCLogInterceptor {
  pb::Timestamp started_at;

 public:
  RPCLogInterceptor(ServiceProvider& provider) { intercept(provider); }

  RPCLogInterceptor() = default;

  void intercept(ServiceProvider& provider) {
    provider.add_interceptor_before([this](Context* context) { started_at = current_time(); });

    provider.add_interceptor_after([this](Context* context) {
      auto took = pb::TimeUtil::DurationToMilliseconds(current_time() - started_at);
      info("{};{};{}ms", context->request->RoutingKey(), StatusCode_Name(context->status.code()),
           took);
    });
  }
};  // RPCLogInterceptor

}  // namespace is