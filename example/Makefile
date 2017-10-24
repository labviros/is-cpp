CXX = clang++
CXXFLAGS += -std=c++11
DEBUGFLAGS = -Wall -Werror -g # -fsanitize=address -fno-omit-frame-pointer 
LDFLAGS += -L/usr/local/lib -I/usr/local/include `pkg-config --libs protobuf librabbitmq libSimpleAmqpClient`\
           -Wl,--no-as-needed -Wl,--as-needed -ldl -lboost_system -lboost_chrono
PROTOC = protoc

IS_PROTOS_PATH = /usr/local/include/is/msgs/
LOCAL_PROTOS_PATH = ./msgs/

vpath %.proto $(LOCAL_PROTOS_PATH)
vpath %.proto $(IS_PROTOS_PATH)

all: hello

hello: hello.pb.o hello.o common.pb.o
	$(CXX) $(DEBUGFLAGS) $^ $(LDFLAGS) -o $@

.PRECIOUS: %.pb.cc
%.pb.cc: %.proto
	$(PROTOC) -I $(LOCAL_PROTOS_PATH) -I $(IS_PROTOS_PATH) --cpp_out=. $<

clean:
	rm -f *.o *.pb.cc *.pb.h hello 