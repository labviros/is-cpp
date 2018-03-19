#pragma once

#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/util/message_differencer.h>
#include <google/protobuf/util/time_util.h>
#include <is/msgs/common.pb.h>
#include <spdlog/fmt/ostr.h>
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
#include "logger.hpp"

// Enable logging for protobuf messages
namespace google {
namespace protobuf {
inline std::ostream& operator<<(std::ostream& os, google::protobuf::Message const& m) {
  return os << m.ShortDebugString();
}
}  // namespace protobuf
}  // namespace google

namespace is {

namespace po = boost::program_options;
namespace rmq = AmqpClient;
namespace pb {
using namespace google::protobuf::util;
using namespace google::protobuf;
}  // namespace pb

// Returns a unique id
std::string make_random_uid();

// Returns the machine hostname
std::string hostname();

// Tag to identify AMQP consumers. The hostname is used because normally container orchestration
// tools set the container hostname to be its id. The uid part is added to avoid name collisions
// when running outside a container.
std::string consumer_id();

// Return the timestamp that represents the current time with nanosecond precision in relation
// to the 1970/1/1 epoch.
pb::Timestamp current_time();

// Add key value pair to the message header
template <typename T>
void add_header(rmq::BasicMessage::ptr_t const& message, std::string const& key, T const& value);

/* =================================
   Serialization / Deserialization
   ================================= */

// Check if the envelope contains a protobuf payload
bool is_protobuf(rmq::Envelope::ptr_t const& envelope);

// Check if the envelope contains a json payload
bool is_json(rmq::Envelope::ptr_t const& envelope);

// Tries to deserializes the protobuf payload inside the envelope as having the schema T
template <typename T>
boost::optional<T> unpack_proto(rmq::Envelope::ptr_t const& envelope);

// Tries to deserializes the json payload inside the envelope as having the schema T
template <typename T>
boost::optional<T> unpack_json(rmq::Envelope::ptr_t const& envelope);

// Tries to deserialize the contents of an envelope based on the content-type specified. If no
// content-type is provided the implementation will try to deserialize it using all the supported
// types (First JSON then Protobuf).
template <typename T>
boost::optional<T> unpack(rmq::Envelope::ptr_t const& envelope);

// Serializes the object T to the protobuf binary protocol and use it as the message payload.
template <typename T>
rmq::BasicMessage::ptr_t pack_proto(T const& object);

// Serializes the object T to the json protocol and use it as the message payload.
template <typename T>
rmq::BasicMessage::ptr_t pack_json(T const& object);

/* ===========
    Transport
   ============ */

rmq::Channel::ptr_t make_channel(std::string const& uri);

std::string declare_queue(rmq::Channel::ptr_t const& channel, std::string const& queue,
                          std::string const& tag, bool exclusive = true, int prefetch_n = -1,
                          int queue_size = 64);

// Declare a queue using reasonable defaults
std::string declare_queue(rmq::Channel::ptr_t const& channel, bool exclusive = true,
                          int prefetch_n = -1, int queue_size = 64);

void subscribe(rmq::Channel::ptr_t const& channel, std::string const& queue,
               std::string const& topic);

void subscribe(rmq::Channel::ptr_t const& channel, std::string const& queue,
               std::vector<std::string> const& topics);

void unsubscribe(rmq::Channel::ptr_t const& channel, std::string const& queue,
                 std::string const& topic);

void unsubscribe(rmq::Channel::ptr_t const& channel, std::string const& queue,
                 std::vector<std::string> const& topics);

void publish(rmq::Channel::ptr_t const& channel, std::string const& topic,
             rmq::BasicMessage::ptr_t const& message);

void publish(rmq::Channel::ptr_t const& channel, std::string const& topic,
             pb::Message const& proto);

rmq::BasicMessage::ptr_t prepare_request(std::string const& queue, pb::Message const& proto);

std::string request(rmq::Channel::ptr_t const& channel, std::string const& queue,
                    std::string const& endpoint, pb::Message const& proto);

rmq::Envelope::ptr_t consume(rmq::Channel::ptr_t const& channel);

rmq::Envelope::ptr_t consume_for(rmq::Channel::ptr_t const& channel, pb::Duration const& duration);

rmq::Envelope::ptr_t consume_until(rmq::Channel::ptr_t const& channel,
                                   pb::Timestamp const& deadline);

rmq::Envelope::ptr_t consume(rmq::Channel::ptr_t const& channel, std::string const& tag);

rmq::Envelope::ptr_t consume_for(rmq::Channel::ptr_t const& channel, std::string const& tag,
                                 pb::Duration const& duration);

rmq::Envelope::ptr_t consume_until(rmq::Channel::ptr_t const& channel, std::string const& tag,
                                   pb::Timestamp const& timestamp);

/* =========
    File IO
   ========= */

void save_to_json(std::string const& filename, pb::Message const& message);

template <typename T>
boost::optional<T> load_from_json(std::string const& filename);

/* =================
    Program Options
   ================= */

po::variables_map parse_program_options(int argc, char** argv,
                                        po::options_description const& user_options = {});

}  // namespace is

// ===== Template Imlementations ==========
namespace is {

template <typename T>
void add_header(rmq::BasicMessage::ptr_t const& message, std::string const& key, T const& value) {
  auto table = message->HeaderTableIsSet() ? message->HeaderTable() : rmq::Table();
  table.emplace(rmq::TableKey(key), rmq::TableValue(value));
  message->HeaderTable(table);
}

template <typename T>
boost::optional<T> unpack_proto(rmq::Envelope::ptr_t const& envelope) {
  T object;
  if (!object.ParseFromString(envelope->Message()->Body())) return boost::none;
  return object;
}

template <typename T>
boost::optional<T> unpack_json(rmq::Envelope::ptr_t const& envelope) {
  T object;
  if (!pb::JsonStringToMessage(envelope->Message()->Body(), &object).ok()) return boost::none;
  return object;
}

template <typename T>
boost::optional<T> unpack(rmq::Envelope::ptr_t const& envelope) {
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

template <typename T>
rmq::BasicMessage::ptr_t pack_proto(T const& object) {
  std::string packed;
  object.SerializeToString(&packed);
  auto message = rmq::BasicMessage::Create(packed);
  message->ContentType("application/x-protobuf");
  return message;
}

template <typename T>
rmq::BasicMessage::ptr_t pack_json(T const& object) {
  pb::JsonPrintOptions options;
  options.always_print_primitive_fields = true;
  std::string packed;
  pb::MessageToJsonString(object, &packed, options);
  auto message = rmq::BasicMessage::Create(packed);
  message->ContentType("application/json");
  return message;
}

template <typename T>
boost::optional<T> load_from_json(std::string const& filename) {
  T message;
  std::ifstream in(filename);
  std::string buffer((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  return is::pb::JsonStringToMessage(buffer, &message).ok() ? boost::optional<T>(message)
                                                            : boost::none;
}

}  // namespace is
