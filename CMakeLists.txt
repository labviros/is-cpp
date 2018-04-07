cmake_minimum_required(VERSION 3.0)
project(is VERSION 0.0.1 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

include(GNUInstallDirs)

find_library(rabbitmq_LIBRARIES NAMES rabbitmq)
find_path(rabbitmq_INCLUDE_DIRS NAMES amqp.h)
message("!! Rabbitmq LIBS=${rabbitmq_LIBRARIES}")

find_library(SimpleAmqpClient_LIBRARIES NAMES SimpleAmqpClient)
find_path(SimpleAmqpClient_INCLUDE_DIRS NAMES SimpleAmqpClient)
message("!! SimpleAmqpClient LIBS=${SimpleAmqpClient_LIBRARIES}")

find_package(Protobuf REQUIRED)
find_package(Boost COMPONENTS chrono program_options system REQUIRED)
find_package(spdlog REQUIRED)
find_package(ismsgs REQUIRED)
find_package(prometheus-cpp REQUIRED)

add_library(is 
  is/core.cpp 
  is/rpc/service-provider.cpp
  is/rpc/interceptors/log-interceptor.cpp
  is/rpc/interceptors/metrics-interceptor.cpp
)

set_property(TARGET is PROPERTY CXX_STANDARD 11)

target_link_libraries(
  is 
 PUBLIC
  ${SimpleAmqpClient_LIBRARIES} 
  ${rabbitmq_LIBRARIES} 
  ${Boost_LIBRARIES} 
  ismsgs::ismsgs
  prometheus-cpp::prometheus-cpp
  ${PROTOBUF_LIBRARIES} 
)

target_include_directories(
  is 
 PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/is>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  ${SimpleAmqpClient_INCLUDE_DIRS} 
  ${PROTOBUF_INCLUDE_DIRS} 
  ${Boost_INCLUDE_DIRS} 
  ${rabbitmq_INCLUDE_DIRS}
) 

install(
  TARGETS is
  EXPORT isTargets
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

#install config
install(
  EXPORT isTargets
  FILE isConfig.cmake
  NAMESPACE is::
  DESTINATION lib/cmake/is
)

# install headers
install(
  DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/is"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# # register project in CMake user registry
export(PACKAGE ${PROJECT_NAME})
