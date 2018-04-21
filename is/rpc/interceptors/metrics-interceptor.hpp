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
  struct ServiceMetrics {
    prometheus::Counter* duration;
    prometheus::Counter* req;
    prometheus::Counter* ok;
    prometheus::Counter* de;
  };

  pb::Timestamp started_at;
  std::unordered_map<std::string, ServiceMetrics> service_metrics;

  std::shared_ptr<prometheus::Registry> registry;
  prometheus::Family<prometheus::Counter>* duration_family;
  prometheus::Family<prometheus::Counter>* req_family;
  prometheus::Family<prometheus::Counter>* ok_family;
  prometheus::Family<prometheus::Counter>* de_family;

 public:
  MetricsInterceptor(std::shared_ptr<prometheus::Registry> const& registry);
  InterceptorConcept* copy() const;

  void before_call(Context* context);
  void after_call(Context* context);
};

}  // namespace is
