#include <is/core.hpp>
#include <is/rpc.hpp>
#include <is/rpc/interceptors/log-interceptor.hpp>
#include <is/rpc/interceptors/metrics-interceptor.hpp>
#include "hello.pb.h"

int main(int argc, char** argv) {
  using hello::GreeterReply;
  using hello::GreeterRequest;

  std::string uri, service_name;

  // Define our parser to read command line arguments
  is::po::options_description opts("Options");
  auto opt_add = opts.add_options();
  opt_add("uri,u", is::po::value<std::string>(&uri)->required(), "amqp broker uri");
  opt_add("name,n", is::po::value<std::string>(&service_name)->default_value("Greeter"),
          "service name");
  is::parse_program_options(argc, argv, opts);

  is::ServiceProvider provider;
  provider.add_interceptor<is::LogInterceptor>();      // enable logging for our services
  provider.add_interceptor<is::MetricsInterceptor>();  // enable metrics for our services
  provider.connect(uri);                               // Connect to the AMQP broker

  // Declares a queue with "service_name" on the broker to storage service requests.
  auto tag = provider.declare_queue(service_name);

  // Delegate the callback that is going to be executed when we receive a Hello request.
  provider.delegate<GreeterRequest, GreeterReply>(
      tag, "Hello", [](auto const& request, auto* reply) {
        // Check if request data is valid
        if (request.name().empty())
          return is::make_status(is::StatusCode::INVALID_ARGUMENT, "Name can't be empty");
        // Fill our reply
        *reply->mutable_greeting() = fmt::format("Hello {}, how are you ? :)", request.name());
        // Signal that everything was fine with the request
        return is::make_status(is::StatusCode::OK);
      });

  // If one of the services throw (like this one) the caller (client) will be notified
  provider.delegate<GreeterRequest, GreeterReply>(
      tag, "Throw", [](auto const& request, auto* reply) {
        throw std::runtime_error("My exception message");
        return is::make_status(is::StatusCode::OK);
      });

  is::info("Listening for incoming requests...");
  provider.run();  // blocks forever
}