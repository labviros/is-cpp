#include "metrics-interceptor.hpp"

namespace is {

void MetricsInterceptor::setup_latency_histograms() {
  auto name = "rpc_duration_ms";
  latency_family = &prometheus::BuildHistogram().Name(name).Register(*registry);
}

void MetricsInterceptor::setup_code_counters() {
  for (int i = 0; i < common::StatusCode_ARRAYSIZE; ++i) {
    auto code = common::StatusCode_Name(StatusCode(i));
    std::transform(code.begin(), code.end(), code.begin(), ::tolower);
    auto metric = fmt::format("code_{}_total", code);
    auto* family = &prometheus::BuildCounter().Name(metric).Register(*registry);
    code_counters[i] = &(family->Add({}));
  }
}

MetricsInterceptor::MetricsInterceptor(std::shared_ptr<prometheus::Registry> const& reg)
    : registry(reg) {
  setup_latency_histograms();
  setup_code_counters();
  set_latency_boundaries(5, 10, 0.7);
}

InterceptorConcept* MetricsInterceptor::copy() const {
  return new MetricsInterceptor(*this);
}

void MetricsInterceptor::set_latency_boundaries(int n_buckets, double scale, double growth_factor) {
  std::vector<double> distribution(n_buckets);
  for (int n = 0; n < n_buckets; ++n) {
    distribution[n] = scale * std::exp(n * growth_factor);
  }
  boundaries = prometheus::Histogram::BucketBoundaries{distribution};
}

void MetricsInterceptor::before_call(Context* context) {
  started_at = current_time();
  auto topic = context->topic();
  auto key_value = latency_histograms.find(topic);
  if (key_value != latency_histograms.end()) {
    latency_histogram = key_value->second;
  } else {
    latency_histogram = &latency_family->Add({{"service", fmt::format("{}", topic)}}, boundaries);
    latency_histograms.emplace(topic, latency_histogram);
  }
}

void MetricsInterceptor::after_call(Context* context) {
  auto took = pb::TimeUtil::DurationToMilliseconds(current_time() - started_at);
  latency_histogram->Observe(took);
  code_counters[context->status().code()]->Increment();
}

}  // namespace is
