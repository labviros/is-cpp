#include "metrics-interceptor.hpp"

namespace is {

MetricsInterceptor::MetricsInterceptor(std::shared_ptr<prometheus::Registry> const& reg)
    : registry(reg) {
  duration_family = &prometheus::BuildCounter()
                         .Name("duration_total")
                         .Help("Sum of request duration in milliseconds")
                         .Register(*registry);

  req_family = &prometheus::BuildCounter()
                    .Name("requests_total")
                    .Help("Number of requests processed")
                    .Register(*registry);

  ok_family = &prometheus::BuildCounter()
                   .Name("ok_total")
                   .Help("Number of OK responses")
                   .Register(*registry);

  de_family = &prometheus::BuildCounter()
                   .Name("de_total")
                   .Help("Number of DEADLINE_EXCEEDED responses")
                   .Register(*registry);
}

InterceptorConcept* MetricsInterceptor::copy() const {
  return new MetricsInterceptor(*this);
}

void MetricsInterceptor::before_call(Context*) {
  started_at = current_time();
}

void MetricsInterceptor::after_call(Context* context) {
  auto duration = pb::TimeUtil::DurationToMilliseconds(current_time() - started_at);

  auto topic = context->topic();
  auto key_value = service_metrics.find(topic);

  if (key_value == service_metrics.end()) {
    ServiceMetrics metrics;
    metrics.duration = &duration_family->Add({{"service", fmt::format("{}", topic)}});
    metrics.req = &req_family->Add({{"service", fmt::format("{}", topic)}});
    metrics.ok = &ok_family->Add({{"service", fmt::format("{}", topic)}});
    metrics.de = &de_family->Add({{"service", fmt::format("{}", topic)}});
    auto iter_and_ok = service_metrics.emplace(topic, metrics);
    if (!iter_and_ok.second) {
      is::error("Failed to insert metric into map");
      return;
    }
    key_value = iter_and_ok.first;
  }

  auto metrics = key_value->second;
  metrics.duration->Increment(duration);
  metrics.req->Increment();
  if (context->status().code() == StatusCode::OK) {
    metrics.ok->Increment();
  } else if (context->status().code() == StatusCode::DEADLINE_EXCEEDED) {
    metrics.de->Increment();
  }
}

}  // namespace is
