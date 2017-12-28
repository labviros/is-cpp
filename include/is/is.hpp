#ifndef __IS_HPP__
#define __IS_HPP__

#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/util/message_differencer.h>
#include <google/protobuf/util/time_util.h>
#include <is/msgs/common.pb.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>
#include <zipkin/opentracing.h>
#include <boost/asio.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <fstream>

// Enable logging for protobuf messages
namespace google {
namespace protobuf {
std::ostream& operator<<(std::ostream& os, google::protobuf::Message const& m) {
  return os << m.ShortDebugString();
}
}  // namespace protobuf
}  // namespace google

namespace is {

namespace pb {
using namespace google::protobuf::util;
using namespace google::protobuf;
}  // namespace pb

namespace rmq {
using namespace AmqpClient;
}

namespace po = boost::program_options;

using common::Status;
using common::StatusCode;

inline std::shared_ptr<spdlog::logger> logger() {
  static auto ptr = [] {
    auto ptr = spdlog::stdout_color_mt("is");
    ptr->set_pattern("[%L][%t][%d-%m-%Y %H:%M:%S:%e] %v");
    return ptr;
  }();
  return ptr;
}

template <class... Args>
inline void info(Args&&... args) {
  logger()->info(args...);
}

template <class... Args>
inline void warn(Args&&... args) {
  logger()->warn(args...);
}

template <class... Args>
inline void error(Args&&... args) {
  logger()->error(args...);
}

template <class... Args>
inline void critical(Args&&... args) {
  logger()->critical(args...);
  std::exit(-1);
}

inline int set_loglevel(char level) {
  if (level == 'i')
    spdlog::set_level(spdlog::level::info);
  else if (level == 'w')
    spdlog::set_level(spdlog::level::warn);
  else if (level == 'e')
    spdlog::set_level(spdlog::level::err);
  else
    return -1;  // failed
  return 0;     // success
}

inline std::string make_random_uid() {
  return boost::uuids::to_string(boost::uuids::random_generator()());
}

inline std::string hostname() {
  return boost::asio::ip::host_name();
}

// Tag to identify AMQP consumers. The hostname is used because normally container orchestration
// tools set the container hostname to be its id. The uid part is added to avoid name collisions
// when running outside a container.
inline std::string consumer_id() {
  return fmt::format("{}/{}", hostname(), make_random_uid());
}

inline pb::Timestamp current_time() {
  const boost::posix_time::ptime epoch(boost::gregorian::date(1970, 1, 1));
  auto nanos = (boost::posix_time::microsec_clock::universal_time() - epoch).total_nanoseconds();
  pb::Timestamp timestamp;
  timestamp.set_seconds(nanos / 1000000000);
  timestamp.set_nanos(nanos % 1000000000);
  return timestamp;
}

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

inline Status make_status(StatusCode code, std::string const& why = "") {
  Status status;
  status.set_code(code);
  status.set_why(why);
  return status;
}

inline Status rpc_status(rmq::Envelope::ptr_t const& envelope) {
  if (envelope != nullptr && envelope->Message()->HeaderTableIsSet()) {
    Status status;
    pb::JsonStringToMessage(envelope->Message()->HeaderTable()["rpc-status"].GetString(), &status);
    return status;
  }
  return make_status(StatusCode::UNKNOWN);
}

template <typename T>
inline boost::optional<T> unpack_proto(rmq::Envelope::ptr_t const& envelope) {
  T object;
  if (!object.ParseFromString(envelope->Message()->Body())) return boost::none;
  return object;
}

template <typename T>
inline boost::optional<T> unpack_json(rmq::Envelope::ptr_t const& envelope) {
  T object;
  if (!pb::JsonStringToMessage(envelope->Message()->Body(), &object).ok()) return boost::none;
  return object;
}

template <typename T>
inline rmq::BasicMessage::ptr_t pack_proto(T const& object) {
  std::string packed;
  object.SerializeToString(&packed);
  auto message = rmq::BasicMessage::Create(packed);
  message->ContentType("application/x-protobuf");
  return message;
}

template <typename T>
inline rmq::BasicMessage::ptr_t pack_json(T const& object) {
  pb::JsonPrintOptions options;
  options.always_print_primitive_fields = true;
  std::string packed;
  pb::MessageToJsonString(object, &packed, options);
  auto message = rmq::BasicMessage::Create(packed);
  message->ContentType("application/json");
  return message;
}

inline bool is_protobuf(rmq::Envelope::ptr_t const& envelope) {
  return envelope->Message()->ContentTypeIsSet() &&
         envelope->Message()->ContentType() == "application/x-protobuf";
}

inline bool is_json(rmq::Envelope::ptr_t const& envelope) {
  return envelope->Message()->ContentTypeIsSet() &&
         envelope->Message()->ContentType() == "application/json";
}

// Deserialize the contents of an envelope based on the content-type specified. If no content-type
// is provided the implementation will try to deserialize it using all the supported types (JSON and
// Protobuf).
template <typename T>
inline boost::optional<T> unpack(rmq::Envelope::ptr_t const& envelope) {
  if (is_protobuf(envelope)) { return unpack_proto<T>(envelope); }
  if (is_json(envelope)) { return unpack_json<T>(envelope); }
  // User didn't provide a valid type, try all.
  auto unpacked = unpack_json<T>(envelope);
  if (unpacked) {
    envelope->Message()->ContentType("application/json");
    return unpacked;
  }
  unpacked = unpack_proto<T>(envelope);
  if (unpacked) { envelope->Message()->ContentType("application/x-protobuf"); }
  return unpacked;
}

inline void subscribe(rmq::Channel::ptr_t const& channel, std::string const& queue,
                      std::string const& topic) {
  channel->BindQueue(queue, "is", topic);
}

inline void subscribe(rmq::Channel::ptr_t const& channel, std::string const& queue,
                      std::vector<std::string> const& topics) {
  for (auto&& topic : topics)
    subscribe(channel, queue, topic);
}

inline void unsubscribe(rmq::Channel::ptr_t const& channel, std::string const& queue,
                        std::string const& topic) {
  channel->UnbindQueue(queue, "is", topic);
}

inline void unsubscribe(rmq::Channel::ptr_t const& channel, std::string const& queue,
                        std::vector<std::string> const& topics) {
  for (auto&& topic : topics)
    unsubscribe(channel, queue, topic);
}

inline std::string declare_queue(rmq::Channel::ptr_t const& channel, std::string const& queue,
                                 std::string const& tag, bool exclusive = true, int prefetch_n = -1,
                                 int queue_size = 64) {
  bool noack = prefetch_n == -1 ? true : false;
  rmq::Table headers{{rmq::TableKey("x-max-length"), rmq::TableValue(queue_size)}};
  channel->DeclareExchange("is", "topic");
  channel->DeclareQueue(queue, /*passive*/ false, /*durable*/ false, exclusive,
                        /*autodelete*/ true, headers);
  channel->BasicConsume(queue, tag, /*nolocal*/ false, noack, exclusive, prefetch_n);
  subscribe(channel, queue, queue);
  return tag;
}

// Declare a queue using reasonable defaults
inline std::string declare_queue(rmq::Channel::ptr_t const& channel, bool exclusive = true,
                                 int prefetch_n = -1, int queue_size = 64) {
  auto tag = consumer_id();
  return declare_queue(channel, tag, tag, exclusive, prefetch_n, queue_size);
}

inline void publish(rmq::Channel::ptr_t const& channel, std::string const& topic,
                    rmq::BasicMessage::ptr_t const& message) {
  if (!message->TimestampIsSet()) {
    message->Timestamp(pb::TimeUtil::TimestampToMilliseconds(current_time()));
  }
  channel->BasicPublish("is", topic, message);
}

inline void publish(rmq::Channel::ptr_t const& channel, std::string const& topic,
                    pb::Message const& proto) {
  publish(channel, topic, pack_proto(proto));
}

inline rmq::BasicMessage::ptr_t prepare_request(std::string const& queue,
                                                pb::Message const& proto) {
  auto message = is::pack_proto(proto);
  message->ReplyTo(queue);
  message->CorrelationId(make_random_uid());
  return message;
}

inline std::string request(rmq::Channel::ptr_t const& channel, std::string const& queue,
                           std::string const& endpoint, pb::Message const& proto) {
  auto message = prepare_request(queue, proto);
  publish(channel, endpoint, message);
  return message->CorrelationId();
}

inline rmq::Envelope::ptr_t consume(rmq::Channel::ptr_t const& channel) {
  return channel->BasicConsumeMessage();
}

inline rmq::Envelope::ptr_t consume_for(rmq::Channel::ptr_t const& channel,
                                        pb::Duration const& duration) {
  rmq::Envelope::ptr_t envelope;
  auto ms = pb::TimeUtil::DurationToMilliseconds(duration);
  if (ms >= 0) { channel->BasicConsumeMessage(envelope, ms); }
  return envelope;
}

inline rmq::Envelope::ptr_t consume_until(rmq::Channel::ptr_t const& channel,
                                          pb::Timestamp const& timestamp) {
  return consume_for(channel, timestamp - current_time());
}

inline rmq::Envelope::ptr_t consume(rmq::Channel::ptr_t const& channel, std::string const& tag) {
  return channel->BasicConsumeMessage(tag);
}

inline rmq::Envelope::ptr_t consume_for(rmq::Channel::ptr_t const& channel, std::string const& tag,
                                        pb::Duration const& duration) {
  rmq::Envelope::ptr_t envelope;
  auto ms = pb::TimeUtil::DurationToMilliseconds(duration);
  if (ms >= 0) { channel->BasicConsumeMessage(tag, envelope, ms); }
  return envelope;
}

inline rmq::Envelope::ptr_t consume_until(rmq::Channel::ptr_t const& channel,
                                          std::string const& tag, pb::Timestamp const& timestamp) {
  return consume_for(channel, tag, timestamp - current_time());
}

template <typename T>
inline void add_header(rmq::BasicMessage::ptr_t const& message, std::string const& key,
                       T const& value) {
  auto table = message->HeaderTableIsSet() ? message->HeaderTable() : rmq::Table();
  table.emplace(rmq::TableKey(key), rmq::TableValue(value));
  message->HeaderTable(table);
}

struct Context {
  rmq::BasicMessage::ptr_t reply;
  rmq::Envelope::ptr_t request;
  Status status;

  Context(rmq::Envelope::ptr_t const& envelope) : request(envelope) {}

  void propagate() const {
    if (request->Message()->CorrelationIdIsSet()) {
      reply->CorrelationId(request->Message()->CorrelationId());
    }
    std::string packed_status;
    pb::MessageToJsonString(status, &packed_status);
    add_header(reply, "rpc-status", packed_status);
  }
};

class ServiceProvider {
  using MethodHandler = std::function<void(Context*)>;
  using Interceptor = std::function<void(Context*)>;

  std::unordered_map<std::string, MethodHandler> methods;
  rmq::Channel::ptr_t channel;
  std::string tag;

  std::vector<Interceptor> before;
  std::vector<Interceptor> after;

  void run_interceptors(std::vector<Interceptor> const& interceptors, Context* context) const {
    for (auto&& interceptor : interceptors)
      interceptor(context);
  }

 public:
  ServiceProvider() : tag(consumer_id()) {}
  void connect(std::string const& uri) { channel = rmq::Channel::CreateFromUri(uri); }
  void connect(rmq::Channel::ptr_t const& new_channel) { channel = new_channel; }

  rmq::Channel::ptr_t const& get_channel() const { return channel; }
  std::string const& get_tag() const { return tag; }

  std::string declare_queue(std::string name, std::string const& id = "",
                            int queue_size = 64) const {
    auto exclusive = !id.empty();
    if (exclusive) name += '.' + id;
    channel->DeclareExchange("is", "topic");
    rmq::Table headers{{rmq::TableKey("x-max-length"), rmq::TableValue(queue_size)}};
    channel->DeclareQueue(name, /*passive*/ false, /*durable*/ false, exclusive,
                          /*autodelete*/ true, headers);
    channel->BasicConsume(name, tag, /*nolocal*/ true, /*noack*/ false, exclusive);
    return name;
  }

  // This interceptor will be called before the service implementation
  void add_interceptor_before(Interceptor&& interceptor) { before.emplace_back(interceptor); }
  // This interceptor will be called after the service implementation
  void add_interceptor_after(Interceptor&& interceptor) { after.emplace_back(interceptor); }

  template <typename Request, typename Reply>
  void delegate(std::string const& queue, std::string const& name,
                std::function<Status(Request, Reply*)> method) {
    std::string binding = queue + '.' + name;
    channel->BindQueue(queue, "is", binding);

    methods.emplace(binding, [=](Context* context) {
      boost::optional<Request> request = unpack<Request>(context->request);
      Reply reply;

      if (request) {
        try {
          context->status = method(*request, &reply);
        } catch (std::exception const& e) {
          auto reason = fmt::format("Service throwed: '{}'", e.what());
          context->status = make_status(StatusCode::INTERNAL_ERROR, reason);
        }
      } else {
        auto reason =
            fmt::format("Expected type '{}'", Request{}.GetMetadata().descriptor->full_name());
        context->status = make_status(StatusCode::FAILED_PRECONDITION, reason);
      }

      if (context->status.code() == StatusCode::OK) {
        context->reply = is_protobuf(context->request) ? pack_proto(reply) : pack_json(reply);
      } else {
        context->reply = rmq::BasicMessage::Create();
      }

    });
  }

  void serve(rmq::Envelope::ptr_t const& envelope) const {
    Context context(envelope);
    run_interceptors(before, &context);

    auto method = methods.find(envelope->RoutingKey());
    if (method != methods.end()) {
      method->second(&context);

      if (context.request->Message()->ReplyToIsSet()) {
        context.propagate();
        publish(channel, context.request->Message()->ReplyTo(), context.reply);
      }
    }

    channel->BasicAck(envelope);
    run_interceptors(after, &context);
  }

  void run() const {
    for (;;) {
      serve(consume(channel, tag));
    }
  }
};

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
};

class RPCLogInterceptor {
  pb::Timestamp started_at;

 public:
  RPCLogInterceptor(ServiceProvider& provider) { intercept(provider); }

  RPCLogInterceptor() = default;

  void intercept(ServiceProvider& provider) {
    provider.add_interceptor_before([this](Context* context) { started_at = current_time(); });

    provider.add_interceptor_after([this](Context* context) {
      auto took = pb::TimeUtil::DurationToMilliseconds(current_time() - started_at);
      info("{};{};{}ms", context->request->RoutingKey(), StatusCode_Name(context->status.code()),
           took);
    });
  }
};

class RPCTraceInterceptor {
  Tracer tracer;
  std::unique_ptr<ot::Span> span;

 public:
  RPCTraceInterceptor(ServiceProvider& provider, Tracer const& tracer) : tracer(tracer) {
    intercept(provider);
  }

  void intercept(ServiceProvider& provider) {
    provider.add_interceptor_before([this](Context* context) {
      span = tracer.extract(context->request, context->request->RoutingKey());
    });

    provider.add_interceptor_after([this](Context* context) {
      tracer.inject(context->reply, span->context());
      span->Finish();
    });
  }
};

po::variables_map parse_program_options(int argc, char** argv,
                                        po::options_description const& user_options) {
  auto add_common_options = [](po::options_description const& others) {
    po::options_description description("Common");
    auto&& options = description.add_options();
    description.add(others);

    options("help", "show available options");
    options("loglevel", po::value<char>()->default_value('i'),
            "char indicating the desired log level: i[nfo], w[warn], e[error]");
    return description;
  };

  auto print_help = [](po::options_description const& description,
                       std::string const& message = "") {
    std::cout << message << '\n' << description << std::endl;
    std::exit(0);
  };

  auto environment_map = [](std::string env) -> std::string {
    std::string prefix("IS_");

    auto starts_with = [](std::string const& s, std::string const& prefix) {
      return s.compare(0, prefix.size(), prefix) == 0;
    };

    if (!starts_with(env, prefix)) return "";

    auto cropped_to_lower_snake_case = [&](std::string& s) {
      auto first = s.begin();
      std::advance(first, prefix.size());
      std::transform(first, s.end(), first,
                     [](char c) { return c == '_' ? '-' : std::tolower(c); });
      return s.substr(prefix.size());
    };

    return cropped_to_lower_snake_case(env);
  };

  auto description = add_common_options(user_options);
  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, description), vm);
    po::store(po::parse_environment(description, environment_map), vm);
    po::notify(vm);
  } catch (std::exception const& e) {
    auto error = fmt::format("Error parsing program options: {}", e.what());
    print_help(description, error);
  }

  if (vm.count("help")) print_help(description);

  if (set_loglevel(vm["loglevel"].as<char>()))
    print_help(description, fmt::format("Invalid log level '{}'", vm["loglevel"].as<char>()));

  return vm;
}

inline void save_to_json(std::string const& filename, pb::Message const& message) {
  pb::JsonPrintOptions options;
  options.add_whitespace = true;
  options.always_print_primitive_fields = true;
  std::string json;

  pb::MessageToJsonString(message, &json, options);

  std::ofstream file(filename);
  file << json;
  file.close();
}

template <typename T>
inline boost::optional<T> load_from_json(std::string const& filename) {
  T message;
  std::ifstream in(filename);
  std::string buffer((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return is::pb::JsonStringToMessage(buffer, &message).ok() ? boost::optional<T>(message)
                                                            : boost::none;
}
}  // namespace is

#endif  // __IS_HPP__