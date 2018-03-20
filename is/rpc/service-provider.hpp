#pragma once

#include <is/msgs/common.pb.h>
#include "../core.hpp"
#include "context.hpp"
#include "interceptor.hpp"

namespace is {

using common::Status;
using common::StatusCode;

Status make_status(StatusCode code, std::string const& why = "");
Status rpc_status(rmq::Envelope::ptr_t const& envelope);

class ServiceProvider {
  using MethodHandler = std::function<void(Context*)>;

  rmq::Channel::ptr_t channel;
  std::unordered_map<std::string, MethodHandler> methods;
  std::vector<std::unique_ptr<Interceptor>> interceptors;
  std::string tag;

 public:
  ServiceProvider() : tag(consumer_id()) {}
  void connect(std::string const& uri) { channel = make_channel(uri); }
  void connect(rmq::Channel::ptr_t const& ch) { channel = ch; }

  rmq::Channel::ptr_t const& get_channel() const { return channel; }
  std::string const& get_tag() const { return tag; }

  // Setup interceptor to customize the behaviour of this class
  template <typename T, typename... Args>
  void add_interceptor(Args&&... args) {
    interceptors.push_back(std::unique_ptr<T>(new T(std::forward<Args>(args)...)));
  }

  std::string declare_queue(std::string name, std::string const& id = "",
                            int queue_size = 64) const;

  // Bind a function to a particular topic, so everytime a message is received in this topic the
  // function will be called
  template <typename Request, typename Reply>
  void delegate(std::string const& queue, std::string const& name,
                std::function<Status(Request, Reply*)> method);

  //
  void serve(rmq::Envelope::ptr_t const& envelope) const;

  // Blocks the current thread listening for requests
  void run() const;
};

}  // namespace is

// ===== Template Imlementations ==========
namespace is {

template <typename Request, typename Reply>
void ServiceProvider::delegate(std::string const& queue, std::string const& name,
                               std::function<Status(Request, Reply*)> method) {
  std::string binding = fmt::format("{}.{}", queue, name);
  channel->BindQueue(queue, "is", binding);

  methods.emplace(binding, [=](Context* context) {
    boost::optional<Request> request = unpack<Request>(context->request());
    Reply reply;
    Status status;

    if (request) {
      try {
        status = method(*request, &reply);
      } catch (std::exception const& e) {
        auto reason = fmt::format("Call to {} failed with exception\n: '{}'", binding, e.what());
        status = make_status(StatusCode::INTERNAL_ERROR, reason);
      }
    } else {
      auto reason = fmt::format("Expected type '{}' but received something else",
                                Request{}.GetMetadata().descriptor->full_name());
      status = make_status(StatusCode::FAILED_PRECONDITION, reason);
    }

    if (status.code() == StatusCode::OK) {
      context->set_reply(is_protobuf(context->request()) ? pack_proto(reply) : pack_json(reply),
                         status);
    } else {
      context->set_reply(rmq::BasicMessage::Create(), status);
    }
  });
}

}  // namespace is
