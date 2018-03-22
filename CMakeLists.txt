cmake_minimum_required(VERSION 3.0)
project(is VERSION 0.0.1 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

include(GNUInstallDirs)

find_package(PkgConfig REQUIRED)
set(PKG_CONFIG_USE_CMAKE_PREFIX_PATH true)
pkg_check_modules(RmqC REQUIRED librabbitmq)
pkg_check_modules(RmqCpp REQUIRED libSimpleAmqpClient)
find_package(Protobuf REQUIRED)
find_package(Boost COMPONENTS program_options system REQUIRED)
find_package(spdlog REQUIRED)
find_package(ismsgs REQUIRED)
find_package(prometheus-cpp REQUIRED)

add_library(is 
  is/core.cpp 
  is/rpc/service-provider.cpp
  is/rpc/interceptors/log-interceptor.cpp
  is/rpc/interceptors/metrics-interceptor.cpp
)

target_compile_features(is PUBLIC cxx_auto_type)
target_link_libraries(
  is 
 PUBLIC
  ismsgs::ismsgs
  prometheus-cpp::prometheus-cpp
  ${RmqC_LIBRARIES} 
  ${RmqCpp_LIBRARIES} 
  ${PROTOBUF_LIBRARIES} 
  ${Boost_LIBRARIES} 
)

target_include_directories(
  is 
 PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/is>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  ${RmqC_INCLUDE_DIRS} 
  ${RmqCpp_INCLUDE_DIRS} 
  ${PROTOBUF_INCLUDE_DIRS} 
  ${Boost_INCLUDE_DIRS} 
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