#pragma once

#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <zipkin/opentracing.h>
#include <string>

namespace is {

namespace ot = opentracing;
namespace zk = zipkin;

class AmqpCarrier : public ot::TextMapReader, public ot::TextMapWriter {
 public:
  AmqpCarrier(is::rmq::Table& h) : headers(h) {}

  ot::expected<void> Set(ot::string_view key, ot::string_view value) const override {
    headers[static_cast<std::string>(key)] = static_cast<std::string>(value);
    return {};
  }

  ot::expected<void> ForeachKey(
      std::function<ot::expected<void>(ot::string_view key, ot::string_view value)> f)
      const override {
    for (const auto& key_value : headers) {
      if (key_value.first[0] == 'x') {
        auto result = f(key_value.first, key_value.second.GetString());
        if (!result) return result;
      }
    }
    return {};
  }

 private:
  is::rmq::Table& headers;
};

class Tracer {
  std::shared_ptr<ot::Tracer> tracer;

 public:
  Tracer(std::string const& name, std::string const& host = "localhost", uint32_t port = 9411) {
    zk::ZipkinOtTracerOptions options;
    options.collector_host = host;
    options.collector_port = port;
    options.service_name = name;
    tracer = zk::makeZipkinOtTracer(options);
  }
  Tracer(Tracer const&) = default;
  ~Tracer() { tracer->Close(); }

  void inject(rmq::BasicMessage::ptr_t const& message, ot::SpanContext const& context) {
    auto headers = message->HeaderTable();
    auto carrier = AmqpCarrier{headers};
    tracer->Inject(context, carrier);
    message->HeaderTable(headers);
  }

  std::unique_ptr<ot::Span> extract(rmq::Envelope::ptr_t const& envelope, std::string const& name) {
    auto headers = envelope->Message()->HeaderTable();
    auto carrier = AmqpCarrier(headers);
    auto maybe_span = tracer->Extract(carrier);
    return tracer->StartSpan(name, {ot::ChildOf(maybe_span->get())});
  }

  std::unique_ptr<ot::Span> new_span(std::string const& name) { return tracer->StartSpan(name); }
};

}  // namespace is