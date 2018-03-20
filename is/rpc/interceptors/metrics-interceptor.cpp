#include "metrics-interceptor.hpp"

namespace is {

MetricsInterceptor::MetricsInterceptor(int port)
    : exposer(fmt::format("127.0.0.1:{}", port)),
      registry(std::make_shared<prometheus::Registry>()) {
  histogram_family = &prometheus::BuildHistogram().Name("rpc_duration_ms").Register(*registry);
  exposer.RegisterCollectable(registry);
}

void MetricsInterceptor::before_call(Context* context) {
  started_at = current_time();
  auto topic = context->topic();
  auto key_value = histograms.find(topic);
  if (key_value != histograms.end()) {
    histogram = key_value->second;
  } else {
    auto boundaries = prometheus::Histogram::BucketBoundaries{5.0, 15.0, 30.0, 50.0, 100.0};
    histogram = &histogram_family->Add({{"name", fmt::format("{}", topic)}}, boundaries);
    histograms.emplace(topic, histogram);
  }
}

void MetricsInterceptor::after_call(Context* context) {
  auto took = pb::TimeUtil::DurationToMilliseconds(current_time() - started_at);
  histogram->Observe(took);
}

}  // namespace is
