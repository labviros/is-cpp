#include "service-provider.hpp"

namespace is {

Status make_status(StatusCode code, std::string const& why) {
  Status status;
  status.set_code(code);
  status.set_why(why);
  return status;
}

Status rpc_status(rmq::Envelope::ptr_t const& envelope) {
  if (envelope != nullptr && envelope->Message()->HeaderTableIsSet()) {
    Status status;
    pb::JsonStringToMessage(envelope->Message()->HeaderTable()["rpc-status"].GetString(), &status);
    return status;
  }
  return make_status(StatusCode::UNKNOWN);
}

std::string ServiceProvider::declare_queue(std::string name, std::string const& id,
                                           int queue_size) const {
  auto exclusive = !id.empty();
  if (exclusive) name += '.' + id;
  channel->DeclareExchange("is", "topic");
  rmq::Table headers{{rmq::TableKey("x-max-length"), rmq::TableValue(queue_size)}};
  channel->DeclareQueue(name, /*passive*/ false, /*durable*/ false, exclusive,
                        /*autodelete*/ true, headers);
  channel->BasicConsume(name, tag, /*nolocal*/ true, /*noack*/ false, exclusive);
  return name;
}

void ServiceProvider::add_interceptor(Interceptor const& interceptor) {
  interceptors.push_back(interceptor);
}

void ServiceProvider::serve(rmq::Envelope::ptr_t const& envelope) const {
  auto method = methods.find(envelope->RoutingKey());
  if (method != methods.end()) {
    Context context(envelope);
    for (auto&& interceptor : interceptors) {
      before_call(interceptor, &context);
    }

    if (!context.deadline_exceeded()) { method->second(&context); }

    auto exceeded = context.deadline_exceeded();

    for (auto&& interceptor : interceptors) {
      after_call(interceptor, &context);
    }

    if (!exceeded) { publish(channel, context.reply_topic(), context.reply()); }
  }

  channel->BasicAck(envelope);
}

void ServiceProvider::run() const {
  for (;;) {
    serve(consume(channel, tag));
  }
}

}  // namespace is
