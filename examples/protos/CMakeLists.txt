cmake_minimum_required(VERSION 3.0)

find_package(Protobuf 3 REQUIRED)
set(protobuf_MODULE_COMPATIBLE TRUE)

file(GLOB protos "*.proto")
foreach(proto ${protos})
  PROTOBUF_GENERATE_CPP(source header ${proto})
  set(proto_sources "${proto_sources};${source}")
  set(proto_headers "${proto_headers};${header}")
endforeach()

add_library(myprotos ${proto_sources})

target_link_libraries(
  myprotos
 PUBLIC 
  ${Protobuf_LIBRARIES}
)

target_include_directories(
  myprotos
 PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  ${Protobuf_INCLUDE_DIRS}
)