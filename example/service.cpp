#include "../include/is/is.hpp"
#include "hello.pb.h"

using hello::GreeterReply;
using hello::GreeterRequest;

int main(int argc, char** argv) {
  std::string uri, service_name;

  // Define our parser to read command line arguments
  is::po::options_description opts("Options");
  auto opt_add = opts.add_options();
  opt_add("uri,u", is::po::value<std::string>(&uri)->required(), "amqp broker uri");
  opt_add("name,n", is::po::value<std::string>(&service_name)->default_value("Greeter"),
          "service name");
  is::parse_program_options(argc, argv, opts);

  is::ServiceProvider provider;
  provider.connect(uri);  // Connect to the AMQP broker

  // Declares a queue with "service_name" on the broker to storage service requests.
  auto tag = provider.declare_queue(service_name);

  // Delegate the callback that is going to be executed when we receive a Hello request.
  provider.delegate<GreeterRequest, GreeterReply>(
      tag, "Hello", [](auto const& request, auto* reply) {
        is::info("New Hello request");
        // Check if request data is valid
        if (request.name().empty())
          return is::make_status(is::StatusCode::INVALID_ARGUMENT, "Name can't be empty");
        // Fill our reply
        *reply->mutable_greeting() = fmt::format("Hello {}, how are you ? :)", request.name());
        // Signal that everything was fine with the request
        return is::make_status(is::StatusCode::OK);
      });

  is::info("Listening for incoming requests...");
  provider.run();
}