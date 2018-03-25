#pragma once

#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include "../service-provider.hpp"
#include <array>

namespace is {

class MetricsInterceptor : public Interceptor {
  prometheus::Exposer exposer;
  std::shared_ptr<prometheus::Registry> registry;

  std::array<prometheus::Counter*, common::StatusCode_ARRAYSIZE> code_counters;

  pb::Timestamp started_at;
  prometheus::Histogram* latency_histogram;
  std::unordered_map<std::string, prometheus::Histogram*> latency_histograms;

  prometheus::Family<prometheus::Histogram>* histogram_family;

 public:
  MetricsInterceptor(int port = 8080);
  void before_call(Context* context) override;
  void after_call(Context* context) override;
};

}  // namespace is
