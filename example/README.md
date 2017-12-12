Getting Started
-----------------
Before running the examples, make sure you have a AMQP broker ready. The easiest way to do so is running:

```shell
$ docker run -d -m 512M -p 15672:15672 -p 5672:5672 viros/rabbitmq:3
``` 

Will can now check if the broker is working by accessing the management interface at http://localhost:15672. (username/password = "guest")

Protos Folder
--------------
Folder containing our custom protobuf messages.

Helloworld Example
--------------------
This example will show you how to connect, send and receive messages.

Compiling:
```shell
$ make
``` 

Running
```shell
$ ./hello -u amqp://localhost:5672
``` 

Service Example
--------------------
This example will show you how to create a RPC service. An example on how to package the service into a docker container is also provided.

Compiling:
```shell
$ make
``` 

Running:
```shell
$ ./service -u amqp://localhost:5672
``` 

Packaging service into a docker container:
```shell
$ make docker
``` 

Client Example
--------------------
This example will show you how to make a request to a service.
Compiling:

```shell
$ make
``` 

Running:
```shell
$ ./client -u amqp://localhost:5672 -e Hello.Greeter
``` 