CXX = clang++
CXXFLAGS += -std=c++14
LDFLAGS += -L/usr/local/lib -I/usr/local/include `pkg-config --libs protobuf librabbitmq libSimpleAmqpClient`\
           -Wl,--no-as-needed -Wl,--as-needed -ldl -lboost_system -lboost_chrono -lboost_program_options -lismsgs -lprometheus-cpp
PROTOC = protoc

LOCAL_PROTOS_PATH = ../protos/
vpath %.proto $(LOCAL_PROTOS_PATH)

MAINTAINER = viros
SERVICE = greeter
VERSION = 1

all: debug

debug: CXXFLAGS += -g -Wall
debug: LDFLAGS += -fsanitize=address -fno-omit-frame-pointer
debug: service

release: CXXFLAGS += -Wall -Werror -O2
release: service

service: hello.pb.o service.o 
	$(CXX) $(DEBUGFLAGS) $^ $(LDFLAGS) -o $@

.PRECIOUS: %.pb.cc
%.pb.cc: %.proto
	$(PROTOC) -I $(LOCAL_PROTOS_PATH) --cpp_out=. $<

clean:
	rm -f *.o *.pb.cc *.pb.h service

docker: 
	cp ../protos/hello.proto . #copy file here so the docker build enviroment can see it
	docker build -t $(MAINTAINER)/$(SERVICE):$(VERSION) --build-arg=SERVICE=$(SERVICE) .
	rm hello.proto # we can remove it afterwards