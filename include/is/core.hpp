#pragma once

#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>
#include <google/protobuf/util/message_differencer.h>
#include <google/protobuf/util/time_util.h>
#include <is/msgs/common.pb.h>
#include <spdlog/fmt/ostr.h>
#include <boost/asio.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/optional.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

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

using common::Status;
using common::StatusCode;

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

}  // namespace is
