#pragma once

#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include "../rpc.hpp"

namespace is {

class RPCMetricsInterceptor {
  prometheus::Exposer exposer;
  std::shared_ptr<prometheus::Registry> registry;

  prometheus::Family<prometheus::Histogram>& family;
  std::unordered_map<std::string, prometheus::Histogram*> histograms;

  pb::Timestamp started_at;
  prometheus::Histogram* histogram;

 public:
  RPCMetricsInterceptor(ServiceProvider& provider, int port = 8080) : RPCMetricsInterceptor(port) {
    intercept(provider);
  }

  RPCMetricsInterceptor(int port = 8080)
      : exposer(fmt::format("127.0.0.1:{}", port)),
        registry(std::make_shared<prometheus::Registry>()),
        family(prometheus::BuildHistogram().Name("rpc_request_duration").Register(*registry)) {
    exposer.RegisterCollectable(registry);
  }

  void intercept(ServiceProvider& provider) {
    provider.add_interceptor_before([this](Context* context) {
      started_at = current_time();
      auto topic = context->request->RoutingKey();
      auto key_value = histograms.find(topic);
      if (key_value != histograms.end()) {
        histogram = key_value->second;
      } else {
        histogram = &family.Add({{"name", fmt::format("{}", topic)}},
                                prometheus::Histogram::BucketBoundaries{1.0, 2.5, 5.0, 10.0, 25.0,
                                                                        50.0, 100.0, 250.0, 500.0});
        histograms.emplace(topic, histogram);
      }
    });

    provider.add_interceptor_after([this](Context* context) {
      auto took = pb::TimeUtil::DurationToMilliseconds(current_time() - started_at);
      histogram->Observe(took);
    });
  }
};  // RPCMetricsInterceptor

}  // namespace is
