#pragma once

#include <is/msgs/common.pb.h>
#include "../core.hpp"

namespace is {

using common::Status;
using common::StatusCode;

class Context {
  rmq::Envelope::ptr_t req;
  rmq::BasicMessage::ptr_t rep;
  Status stat;

 public:
  Context(rmq::Envelope::ptr_t const& request) : req(request) {}

  rmq::Envelope::ptr_t request() const { return req; }
  rmq::BasicMessage::ptr_t reply() const { return rep; }
  Status status() const { return stat; }

  std::string id() const { return req->Message()->CorrelationId(); }
  std::string topic() const { return req->RoutingKey(); }
  std::string reply_topic() const {
    return req->Message()->ReplyToIsSet() ? req->Message()->ReplyTo()
                                          : fmt::format("{}/reply", topic());
  }

  bool deadline_exceeded() {
    auto deadline = get_deadline(req);
    auto exceeded = deadline && current_time() >= *deadline;
    if (exceeded) stat.set_code(StatusCode::DEADLINE_EXCEEDED);
    return exceeded;
  }

  void set_reply(rmq::BasicMessage::ptr_t const& reply, Status const& status) {
    rep = reply;
    stat = status;
    rep->CorrelationId(id());
    std::string packed_status;
    pb::MessageToJsonString(status, &packed_status);
    add_header(rep, "rpc-status", packed_status);
  }
};

}  // namespace is