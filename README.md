is-cpp
========
Collection of utility functions for the C++ version of the IS architecture. 
The messaging layer is implemented using the AMQP protocol version 0.9.1. AMQP is a binary, application layer protocol, designed to efficiently support a wide variety of messaging applications and communication patterns [[1](https://en.wikipedia.org/wiki/Advanced_Message_Queuing_Protocol)]. The 0.9.1 version requires a broker to connect message consumers and publishers. To better understand the communication model see [[2](https://www.rabbitmq.com/tutorials/amqp-concepts.html)].
Messages payload are serialized using the protobuf binary format. Protocol buffers are Google's language-neutral, platform-neutral, extensible mechanism for serializing structured data [[3](https://developers.google.com/protocol-buffers/)]. For more details on how to define a message schema and use it see [[4](https://developers.google.com/protocol-buffers/docs/proto3)].
 
Dependencies
--------------
Network / Serialization:
- [Boost](http://www.boost.org/)
- [SimpleAmqpClient](https://github.com/alanxz/SimpleAmqpClient)
- [rabbitmq-c](https://github.com/alanxz/rabbitmq-c)
- [protobuf](https://github.com/google/protobuf)

Logging / Metrics / Tracing:
- [spdlog](https://github.com/gabime/spdlog)
- [prometheus-cpp](https://github.com/jupp0r/prometheus-cpp)
- [opentracing-cpp](https://github.com/opentracing/opentracing-cpp)
- [zipkin-cpp-opentracing](https://github.com/rnburn/zipkin-cpp-opentracing)

Computer Vision / Multimedia / Algebra:
- [opencv](https://github.com/opencv/opencv)
- [opencv_contrib](https://github.com/opencv/opencv_contrib)

A installation script is provided to easily install all the dependencies on the linux platform. Just run the command below:
 **(Tested only on Ubuntu versions 14.04 and 16.04)**.
```shell
$ curl -fsSL https://raw.githubusercontent.com/labviros/is-cpp/master/install | bash
```

As explained, AMQP 0.9.1 requires a broker to route messages between clients. We recommend using [RabbitMQ](https://www.rabbitmq.com/).
The broker can be easily instantiated with [Docker](https://www.docker.com/) with the following command:
```shell
$ docker run -d -m 512M -p 15672:15672 -p 5672:5672 viros/rabbitmq:3
```

Using the library
----------------------
Examples describing how to use the library are provided in the examples folder.