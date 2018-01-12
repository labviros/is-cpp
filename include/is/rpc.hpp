#pragma once

#include "core.hpp"

namespace is {

template <typename T>
inline void add_header(rmq::BasicMessage::ptr_t const& message, std::string const& key,
                       T const& value) {
  auto table = message->HeaderTableIsSet() ? message->HeaderTable() : rmq::Table();
  table.emplace(rmq::TableKey(key), rmq::TableValue(value));
  message->HeaderTable(table);
}

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

}  // namespace is