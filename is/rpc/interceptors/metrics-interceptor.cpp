#include "metrics-interceptor.hpp"

namespace is {

MetricsInterceptor::MetricsInterceptor(std::string const& bind_address)
    : exposer(bind_address),
      registry(std::make_shared<prometheus::Registry>()) {
  histogram_family = &prometheus::BuildHistogram().Name("rpc_duration_ms").Register(*registry);

  for (int i = 0; i < common::StatusCode_ARRAYSIZE; ++i) {
    auto code = common::StatusCode_Name(StatusCode(i));
    std::transform(code.begin(), code.end(), code.begin(), ::tolower);
    auto metric = fmt::format("code_{}_total", code);
    auto& counter_family = prometheus::BuildCounter().Name(metric).Register(*registry);
    code_counters[i] = &counter_family.Add({});
  }

  exposer.RegisterCollectable(registry);
}

void MetricsInterceptor::before_call(Context* context) {
  started_at = current_time();
  auto topic = context->topic();
  auto key_value = latency_histograms.find(topic);
  if (key_value != latency_histograms.end()) {
    latency_histogram = key_value->second;
  } else {
    auto boundaries = prometheus::Histogram::BucketBoundaries{5.0, 15.0, 30.0, 50.0, 100.0};
    latency_histogram = &histogram_family->Add({{"service", fmt::format("{}", topic)}}, boundaries);
    latency_histograms.emplace(topic, latency_histogram);
  }
}

void MetricsInterceptor::after_call(Context* context) {
  auto took = pb::TimeUtil::DurationToMilliseconds(current_time() - started_at);
  latency_histogram->Observe(took);
  code_counters[context->status().code()]->Increment();
}

}  // namespace is
