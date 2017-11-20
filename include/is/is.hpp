#ifndef __IS_HPP__
#define __IS_HPP__

#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/util/message_differencer.h>
#include <google/protobuf/util/time_util.h>
#include <is/msgs/common.pb.h>
#include <spdlog/spdlog.h>
#include <boost/asio.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <fstream>

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

inline std::string make_random_uid() {
  return boost::uuids::to_string(boost::uuids::random_generator()());
}
inline std::string hostname() {
  return boost::asio::ip::host_name();
}

// Tag to identify AMQP consumers. The hostname is used because normally container orchestration
// tools set the container hostname to be its id. The uid part is added to avoid name collisions
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

inline Status make_status(StatusCode code, std::string const& why = "") {
  Status status;
  status.set_code(code);
  status.set_why(why);
  return status;
}

inline Status rpc_status(rmq::Envelope::ptr_t const& envelope) {
  if (envelope->Message()->HeaderTableIsSet()) {
    Status status;
    pb::JsonStringToMessage(envelope->Message()->HeaderTable()["rpc-status"].GetString(), &status);
    return status;
  }
  return make_status(StatusCode::UNKNOWN, "Status not set");
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

// Defer the execution of the function to the end of the scope where this class is instantiated
struct Defer {
  const std::function<void()> on_destruction;
  explicit Defer(std::function<void()>&& defered) noexcept : on_destruction(std::move(defered)) {}
  Defer(Defer const&) = delete;
  ~Defer() noexcept { on_destruction(); }
};

//
class ServiceProvider {
  using MethodHandler = std::function<rmq::BasicMessage::ptr_t(rmq::Envelope::ptr_t const&)>;
  std::unordered_map<std::string, MethodHandler> methods;

  rmq::Channel::ptr_t channel;
  std::string tag;

 public:
  ServiceProvider() : tag(consumer_id()) {}

  void connect(std::string const& uri) { channel = rmq::Channel::CreateFromUri(uri); }
  void connect(rmq::Channel::ptr_t const& new_channel) { channel = new_channel; }

  rmq::Channel::ptr_t const& get_underlying_channel() const { return channel; }
  std::string const& get_tag() const { return tag; }

  std::string make_queue(std::string name, std::string const& id) const {
    auto exclusive = !id.empty();
    if (exclusive) name += '.' + id;
    channel->DeclareExchange("is", "topic");
    channel->DeclareQueue(name, /*passive*/ false, /*durable*/ false, exclusive,
                          /*autodelete*/ true);
    channel->BasicConsume(name, tag, /*nolocal*/ true, /*noack*/ false, exclusive);
    return name;
  }

  template <typename Reply, typename Request>
  void delegate(std::string const& queue, std::string const& name,
                std::function<Status(Request, Reply*)> method) {
    std::string binding = queue + '.' + name;
    channel->BindQueue(queue, "is", binding);

    methods.emplace(binding, [=](rmq::Envelope::ptr_t const& envelope) -> rmq::BasicMessage::ptr_t {
      boost::optional<Request> request = unpack<Request>(envelope);

      rmq::BasicMessage::ptr_t message;
      Status status;

      if (request) {
        Reply reply;
        status = method(*request, &reply);
        message = is_protobuf(envelope) ? pack_proto(reply) : pack_json(reply);
      } else {
        status = make_status(StatusCode::INTERNAL_ERROR, "Failed to deserialize payload");
        message = rmq::BasicMessage::Create();
      }

      std::string packed_status;
      pb::MessageToJsonString(status, &packed_status);
      rmq::Table table{{rmq::TableKey("rpc-status"), rmq::TableValue(packed_status)}};
      message->HeaderTable(table);
      return message;
    });
  }

  void serve(rmq::Envelope::ptr_t const& envelope) const {
    const Defer ack([&] { channel->BasicAck(envelope); });
    if (!envelope->Message()->ReplyToIsSet())
      return;  // User did not specified a reply topic, no point in processing this request

    auto method = methods.find(envelope->RoutingKey());
    if (method == methods.end()) return;

    auto message = method->second(envelope);
    if (envelope->Message()->CorrelationIdIsSet())
      message->CorrelationId(envelope->Message()->CorrelationId());

    channel->BasicPublish("is", envelope->Message()->ReplyTo(), message);
  }

  void run() const {
    for (;;) {
      auto envelope = channel->BasicConsumeMessage(tag);
      serve(envelope);
    }
  }
};  // class ServiceProvider

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
                                 std::string const& tag, bool exclusive = true,
                                 int prefetch_n = -1) {
  bool noack = prefetch_n == -1 ? true : false;
  channel->DeclareExchange("is", "topic");
  channel->DeclareQueue(queue, /*passive*/ false, /*durable*/ false, exclusive,
                        /*autodelete*/ true);
  channel->BasicConsume(queue, tag, /*nolocal*/ false, noack, exclusive, prefetch_n);
  subscribe(channel, queue, queue);
  return tag;
}

// Declare a queue using reasonable defaults
inline std::string declare_queue(rmq::Channel::ptr_t const& channel, bool exclusive = true,
                                 int prefetch_n = -1) {
  auto tag = consumer_id();
  return declare_queue(channel, tag, tag, exclusive, prefetch_n);
}

inline void publish(rmq::Channel::ptr_t const& channel, std::string const& topic,
                    pb::Message const& proto) {
  auto message = pack_proto(proto);
  message->Timestamp(pb::TimeUtil::TimestampToMilliseconds(current_time()));
  channel->BasicPublish("is", topic, message);
}

inline std::string request(rmq::Channel::ptr_t const& channel, std::string const& queue,
                           std::string const& endpoint, pb::Message const& proto) {
  auto message = is::pack_proto(proto);
  auto id = is::make_random_uid();
  message->ReplyTo(queue);
  message->CorrelationId(id);
  channel->BasicPublish("is", endpoint, message);
  return id;
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

po::options_description add_common_options(po::options_description const& others) {
  po::options_description description("Common options");
  auto&& options = description.add_options();
  description.add(others);

  options("help,h", "show available options");
  options("uri,u", po::value<std::string>()->required(), "amqp broker uri");
  return description;
}

po::variables_map parse_program_options(int argc, char** argv,
                                        po::options_description const& description) {
  auto print_help = [&] {
    std::cout << description << std::endl;
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

  po::variables_map vm;
  try {
    po::store(po::parse_command_line(argc, argv, description), vm);
    po::store(po::parse_environment(description, environment_map), vm);
    po::notify(vm);
  } catch (std::exception const& e) {
    std::cout << "Error parsing program options: " << e.what() << "\n";
    print_help();
  }

  if (vm.count("help")) print_help();
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
  return is::pb::JsonStringToMessage(buffer, &message).ok() ? boost::optional<T>(message) : boost::none;
}

}  // namespace is

#endif  // __IS_HPP__