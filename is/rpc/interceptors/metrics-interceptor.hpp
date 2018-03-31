#pragma once

#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <array>
#include <cmath>
#include "../service-provider.hpp"

namespace is {

/* Go to: https://github.com/jupp0r/prometheus-cpp/ for more API details;

  prometheus::Exposer exposer("8080");
  auto registry = std::make_shared<prometheus::Registry>();
  exposer.RegisterCollectable(registry);

  auto* family = &prometheus::BuildCounter().Name("requests_total").Register(*registry);
  auto* requests = family->Add({});
  requests->Increment();
*/

class MetricsInterceptor : public InterceptorConcept {
  std::shared_ptr<prometheus::Registry> registry;

  std::array<prometheus::Counter*, common::StatusCode_ARRAYSIZE> code_counters;

  pb::Timestamp started_at;
  prometheus::Histogram* latency_histogram;
  std::unordered_map<std::string, prometheus::Histogram*> latency_histograms;

  prometheus::Family<prometheus::Histogram>* latency_family;
  prometheus::Histogram::BucketBoundaries boundaries;

 public:
  MetricsInterceptor(std::shared_ptr<prometheus::Registry> const& registry);
  InterceptorConcept* copy() const;

  void before_call(Context* context);
  void after_call(Context* context);

  // set exponential latency boundaries
  void set_latency_boundaries(int n_buckets, double scale, double growth_factor);

 private:
  void setup_code_counters();
  void setup_latency_histograms();
};

}  // namespace is
