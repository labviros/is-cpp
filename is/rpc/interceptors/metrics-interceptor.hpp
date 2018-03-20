#pragma once

#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include "../service-provider.hpp"

namespace is {

class MetricsInterceptor : public Interceptor {
  prometheus::Exposer exposer;
  std::shared_ptr<prometheus::Registry> registry;

  std::unordered_map<std::string, prometheus::Histogram*> histograms;
  prometheus::Family<prometheus::Histogram>* histogram_family;
  prometheus::Histogram* histogram;

  pb::Timestamp started_at;

 public:
  MetricsInterceptor(int port = 8080);
  void before_call(Context* context) override;
  void after_call(Context* context) override;
};

}  // namespace is
