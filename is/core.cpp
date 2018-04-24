#include "core.hpp"

namespace is {

std::string make_random_uid() {
  return boost::uuids::to_string(boost::uuids::random_generator()());
}

std::string hostname() {
  return boost::asio::ip::host_name();
}

std::string consumer_id() {
  return fmt::format("{}/{}", hostname(), make_random_uid());
}

bool is_protobuf(rmq::Envelope::ptr_t const& envelope) {
  return envelope->Message()->ContentTypeIsSet() &&
         envelope->Message()->ContentType() == "application/x-protobuf";
}

bool is_json(rmq::Envelope::ptr_t const& envelope) {
  return envelope->Message()->ContentTypeIsSet() &&
         envelope->Message()->ContentType() == "application/json";
}

rmq::Channel::ptr_t make_channel(std::string const& uri) {
  return rmq::Channel::CreateFromUri(uri);
}

void subscribe(rmq::Channel::ptr_t const& channel, std::string const& queue,
               std::string const& topic) {
  channel->BindQueue(queue, "is", topic);
}

void subscribe(rmq::Channel::ptr_t const& channel, std::string const& queue,
               std::vector<std::string> const& topics) {
  for (auto&& topic : topics)
    subscribe(channel, queue, topic);
}

void unsubscribe(rmq::Channel::ptr_t const& channel, std::string const& queue,
                 std::string const& topic) {
  channel->UnbindQueue(queue, "is", topic);
}

void unsubscribe(rmq::Channel::ptr_t const& channel, std::string const& queue,
                 std::vector<std::string> const& topics) {
  for (auto&& topic : topics)
    unsubscribe(channel, queue, topic);
}

std::string declare_queue(rmq::Channel::ptr_t const& channel, std::string const& queue,
                          std::string const& tag, bool exclusive, int prefetch_n, int queue_size) {
  bool noack = prefetch_n == -1 ? true : false;
  rmq::Table headers{{rmq::TableKey("x-max-length"), rmq::TableValue(queue_size)}};
  channel->DeclareExchange("is", "topic");
  channel->DeclareQueue(queue, /*passive*/ false, /*durable*/ false, exclusive,
                        /*autodelete*/ true, headers);
  channel->BasicConsume(queue, tag, /*nolocal*/ false, noack, exclusive, prefetch_n);
  subscribe(channel, queue, queue);
  return tag;
}

std::string declare_queue(rmq::Channel::ptr_t const& channel, bool exclusive, int prefetch_n,
                          int queue_size) {
  auto tag = consumer_id();
  return declare_queue(channel, tag, tag, exclusive, prefetch_n, queue_size);
}

void publish(rmq::Channel::ptr_t const& channel, std::string const& topic,
             rmq::BasicMessage::ptr_t const& message) {
  if (!message->TimestampIsSet()) {
    message->Timestamp(pb::TimeUtil::TimestampToMilliseconds(current_time()));
  }
  channel->BasicPublish("is", topic, message);
}

void publish(rmq::Channel::ptr_t const& channel, std::string const& topic,
             pb::Message const& proto) {
  publish(channel, topic, pack_proto(proto));
}

void set_deadline(rmq::BasicMessage::ptr_t const& request, pb::Timestamp const& deadline) {
  add_header(request, "deadline", deadline.SerializeAsString());
}

boost::optional<pb::Timestamp> get_deadline(rmq::Envelope::ptr_t const& reply) {
  auto headers = reply->Message()->HeaderTable();
  auto header = headers.find("deadline") ;
  if (header == headers.end()) return boost::none;
  pb::Timestamp deadline;
  deadline.ParseFromString(header->second.GetString());
  return deadline;
}

rmq::BasicMessage::ptr_t prepare_request(std::string const& queue, pb::Message const& proto) {
  auto message = is::pack_proto(proto);
  message->ReplyTo(queue);
  message->CorrelationId(make_random_uid());
  return message;
}

std::string request(rmq::Channel::ptr_t const& channel, std::string const& queue,
                    std::string const& endpoint, pb::Message const& proto) {
  auto message = prepare_request(queue, proto);
  publish(channel, endpoint, message);
  return message->CorrelationId();
}

rmq::Envelope::ptr_t consume(rmq::Channel::ptr_t const& channel) {
  return channel->BasicConsumeMessage();
}

rmq::Envelope::ptr_t consume_for(rmq::Channel::ptr_t const& channel, pb::Duration const& duration) {
  rmq::Envelope::ptr_t envelope;
  auto ms = pb::TimeUtil::DurationToMilliseconds(duration);
  if (ms >= 0) { channel->BasicConsumeMessage(envelope, ms); }
  return envelope;
}

rmq::Envelope::ptr_t consume_until(rmq::Channel::ptr_t const& channel,
                                   pb::Timestamp const& timestamp) {
  return consume_for(channel, timestamp - current_time());
}

rmq::Envelope::ptr_t consume(rmq::Channel::ptr_t const& channel, std::string const& tag) {
  return channel->BasicConsumeMessage(tag);
}

rmq::Envelope::ptr_t consume_for(rmq::Channel::ptr_t const& channel, std::string const& tag,
                                 pb::Duration const& duration) {
  rmq::Envelope::ptr_t envelope;
  auto ms = pb::TimeUtil::DurationToMilliseconds(duration);
  if (ms >= 0) { channel->BasicConsumeMessage(tag, envelope, ms); }
  return envelope;
}

rmq::Envelope::ptr_t consume_until(rmq::Channel::ptr_t const& channel, std::string const& tag,
                                   pb::Timestamp const& timestamp) {
  return consume_for(channel, tag, timestamp - current_time());
}

void save_to_json(std::string const& filename, pb::Message const& message) {
  pb::JsonPrintOptions options;
  options.add_whitespace = true;
  options.always_print_primitive_fields = true;
  std::string json;

  pb::MessageToJsonString(message, &json, options);

  std::ofstream file(filename);
  file << json;
  file.close();
}

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

}  // namespace is
