CXX = clang++
CXXFLAGS += -std=c++14 -Werror -Wall
DEBUGFLAGS = # -g -fsanitize=address -fno-omit-frame-pointer 
LDFLAGS += -L/usr/local/lib -I/usr/local/include `pkg-config --libs protobuf librabbitmq libSimpleAmqpClient`\
           -Wl,--no-as-needed -Wl,--as-needed -ldl -lboost_system -lboost_chrono -lboost_program_options -lismsgs
PROTOC = protoc

LOCAL_PROTOS_PATH = ./msgs/

vpath %.proto $(LOCAL_PROTOS_PATH)

all: hello service client

hello: hello.pb.o hello.o 
	$(CXX) $(DEBUGFLAGS) $^ $(LDFLAGS) -o $@

service: hello.pb.o service.o 
	$(CXX) $(DEBUGFLAGS) $^ $(LDFLAGS) -o $@

client: hello.pb.o client.o 
	$(CXX) $(DEBUGFLAGS) $^ $(LDFLAGS) -o $@

.PRECIOUS: %.pb.cc
%.pb.cc: %.proto
	$(PROTOC) -I $(LOCAL_PROTOS_PATH) --cpp_out=. $<

clean:
	rm -f *.o *.pb.cc *.pb.h hello service client
