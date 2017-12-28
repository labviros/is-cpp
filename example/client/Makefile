CXX = clang++
CXXFLAGS += -std=c++14
LDFLAGS += -L/usr/local/lib -I/usr/local/include `pkg-config --libs protobuf librabbitmq libSimpleAmqpClient`\
           -Wl,--no-as-needed -Wl,--as-needed -ldl -lboost_system -lboost_chrono -lboost_program_options -lismsgs\
					 -lprometheus-cpp  -lopentracing -lzipkin -lzipkin_opentracing 
PROTOC = protoc

LOCAL_PROTOS_PATH = ../protos/
vpath %.proto $(LOCAL_PROTOS_PATH)

all: debug

debug: CXXFLAGS += -g -Wall
debug: LDFLAGS += -fsanitize=address -fno-omit-frame-pointer
debug: client

release: CXXFLAGS += -Wall -Werror -O2
release: client

client: hello.pb.o client.o 
	$(CXX) $(DEBUGFLAGS) $^ $(LDFLAGS) -o $@

.PRECIOUS: %.pb.cc
%.pb.cc: %.proto
	$(PROTOC) -I $(LOCAL_PROTOS_PATH) --cpp_out=. $<

clean:
	rm -f *.o *.pb.cc *.pb.h client