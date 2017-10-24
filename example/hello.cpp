#include <SimpleAmqpClient/SimpleAmqpClient.h>  // amqp client library
#include <boost/optional.hpp>                   // value type that can be nullable
#include "../include/is/is.hpp"                 // utility functions for c++
#include "hello.pb.h"                           // auto generated header of our custom message

namespace rmq {
using namespace AmqpClient;
}
using hello::Hello;
using boost::optional;

int main(int argc, char** argv) {
  // Connect to the AMQP broker
  rmq::Channel::ptr_t channel = rmq::Channel::CreateFromUri("amqp://rmq.is:30000");
  is::info("Connected to broker...");

  // Declares a queue on the broker to storage messages "that we are interested"
  std::string queue = is::declare_queue(channel);
  // Tell the broker that we are interested in messages published on the topic "got.weather"
  is::subscribe(channel, queue, "got.weather");

  // Instantiated a Hello message that we defined in msgs/hello.proto
  Hello hello;
  // Fill the message...
  hello.set_text("Winter is coming...");
  hello.set_n(666);
  hello.set_author("John Snow");

  // Publishes our custom message to the "got.weather" topic. As we subscribed to this topic we are
  // also going to receive it.
  is::publish(channel, "got.weather", hello);

  // Note that, a publisher and a consumer of the same topic are normally not on the same process.
  // This is just an usage example...

  // Consume one message from our queue. (Blocking Call)
  rmq::Envelope::ptr_t envelope = channel->BasicConsumeMessage(queue);
  is::info("Received: message on topic=\"{}\"", envelope->RoutingKey());

  // Tries to deserialize message, this can fail if the content of the message does not match the
  // object we are trying to deserialize.
  optional<Hello> maybe_hello = is::unpack<Hello>(envelope);
  if (maybe_hello) {
    // Deserialization was successful
    is::info("text:\"{}\", author:\"{}\", n:{}", maybe_hello->text(), maybe_hello->author(),
             maybe_hello->n());
  }
}