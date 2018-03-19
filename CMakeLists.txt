cmake_minimum_required(VERSION 3.0)
project(is VERSION 0.0.1 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakeDependentOption)

find_package(PkgConfig REQUIRED)
pkg_check_modules(RmqC REQUIRED librabbitmq)
pkg_check_modules(RmqCpp REQUIRED libSimpleAmqpClient)
find_package(Protobuf REQUIRED)
find_package(Boost COMPONENTS program_options system REQUIRED)
find_package(spdlog REQUIRED)
find_package(ismsgs REQUIRED)

add_library(is 
  is/core.cpp 
  is/rpc/service-provider.cpp
  is/rpc/interceptors/log-interceptor.cpp
)

target_compile_features(is PUBLIC cxx_auto_type)

target_link_libraries(
  is 
 PUBLIC
  ismsgs::ismsgs
  ${RmqC_LIBRARIES} 
  ${RmqCpp_LIBRARIES} 
  ${Protobuf_LIBRARIES} 
  ${Boost_LIBRARIES} 
)

target_include_directories(
  is 
 PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/is>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  ${RmqC_INCLUDE_DIRS} 
  ${RmqCpp_INCLUDE_DIRS} 
  ${Protobuf_INCLUDE_DIRS} 
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
