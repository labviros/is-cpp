#include <SimpleAmqpClient/SimpleAmqpClient.h>  // amqp client library used to connect to the broker
#include <boost/optional.hpp>                   // value type that can be nullable
#include "../include/is/is.hpp"                 // utility functions for c++
#include "hello.pb.h"                           // auto generated header of our custom message

using boost::optional;
using hello::Hello;

/* @Simple Publisher/Consumer. This example describes how to:
  - Read input from command line arguments;
  - Subscribe to a topic of interest;
  - Create, serialize and publish a custom message to a topic;
  - Consume and deserialize a message from a topic;

  To run the example with a broker running on localhost:
  $ ./hello --uri amqp://localhost:5672
  or
  $ IS_URI=amqp://localhost:5672 ./hello
*/
int main(int argc, char** argv) {
  std::string uri;

  // Define our parser to read command line arguments
  is::po::options_description opts("Options");
  opts.add_options()("uri,u", is::po::value<std::string>(&uri)->required(), "amqp broker uri");
  is::parse_program_options(argc, argv, opts);

  // Connect to the AMQP broker
  is::rmq::Channel::ptr_t channel = is::rmq::Channel::CreateFromUri(uri);
  is::info("Connected to broker...");

  // Declares a queue on the broker to storage messages "that we are interested".
  // This will return our consumer tag. By default queue and tag names are equal.
  std::string tag = is::declare_queue(channel);
  // Tell the broker that we are interested in messages published on the topic "got.weather"
  is::subscribe(channel, tag, "got.weather");

  // Instantiated a Hello message that we defined in msgs/hello.proto
  Hello hello;
  // Fill the message...
  hello.set_text("Winter is coming...");
  hello.set_n(666);
  hello.set_author("John Snow");

  // Publishes our custom message to the "got.weather" topic. As we subscribed to this topic we are
  // also going to receive it.
  is::publish(channel, "got.weather", hello);
  is::info("Published message");

  // Note that, a publisher and a consumer of the same topic are normally not on the same process.
  // This is just an usage example...

  // Consume one message from our queue. (Blocks forever)
  is::rmq::Envelope::ptr_t envelope = is::consume(channel, tag);
  is::info(R"(New message on topic="{}")", envelope->RoutingKey());

  // Tries to deserialize message, this can fail if the content of the message does not match the
  // object we are trying to deserialize.
  optional<Hello> maybe_hello = is::unpack<Hello>(envelope);
  if (maybe_hello) {
    // Deserialization was successful, print the message we received
    is::info("{}", *maybe_hello);  // Protobuf messages can be passed directly to log functions
  }
}