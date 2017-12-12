#include <is/is.hpp>
#include "hello.pb.h"

using hello::GreeterReply;
using hello::GreeterRequest;

int main(int argc, char** argv) {
  std::string uri, endpoint;

  // Define our parser to read command line arguments
  is::po::options_description opts("Options");
  auto opt_add = opts.add_options();
  opt_add("uri,u", is::po::value<std::string>(&uri)->required(), "amqp broker uri");
  opt_add("endpoint,e", is::po::value<std::string>(&endpoint)->default_value("Greeter.Hello"),
          "service endpoint");
  is::parse_program_options(argc, argv, opts);

  auto channel = is::rmq::Channel::CreateFromUri(uri);
  auto tag = is::declare_queue(channel);

  GreeterRequest request;
  request.set_name("John Snow");

  is::info("Sending request to service {}", endpoint);
  /* Every request has an unique id used to correlate the reply
    with messages we receive. Since we are only sending one request
    we don't need to use it */
  auto id = is::request(channel, tag, endpoint, request);

  // Tries to consume a message with an 1 second timeout
  auto envelope = is::consume_for(channel, tag, is::pb::TimeUtil::SecondsToDuration(1));
  if (envelope == nullptr) {
    is::error("No response from service, is it running?");
    return -1;
  }

  auto status = is::rpc_status(envelope);
  if (status.code() == is::StatusCode::OK) {
    auto reply = is::unpack<GreeterReply>(envelope);
    is::info("Reply: {}", *reply);
  } else {
    is::error("RPC failed: {}", status);
  }
}