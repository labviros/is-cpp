cmake_minimum_required(VERSION 2.8)
add_definitions(-std=c++11)
project(is C CXX)

set(headers
include/is/is.hpp
)

install(FILES ${headers} DESTINATION include/is)
