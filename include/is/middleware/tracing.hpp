#pragma once

#include "../rpc.hpp"
#include "../tracing.hpp"

namespace is {

class RPCTraceInterceptor {
  Tracer tracer;
  std::unique_ptr<ot::Span> span;

 public:
  RPCTraceInterceptor(ServiceProvider& provider, Tracer const& tracer) : tracer(tracer) {
    intercept(provider);
  }

  void intercept(ServiceProvider& provider) {
    provider.add_interceptor_before([this](Context* context) {
      span = tracer.extract(context->request, context->request->RoutingKey());
    });

    provider.add_interceptor_after([this](Context* context) {
      tracer.inject(context->reply, span->context());
      span->Finish();
    });
  }
};

}  // namespace is