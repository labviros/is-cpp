cmake_minimum_required(VERSION 2.8)
add_definitions(-std=c++11)
project(is C CXX)

set(headers
include/is/cli.hpp
include/is/core.hpp
include/is/io.hpp
include/is/is.hpp
include/is/log.hpp
include/is/rpc.hpp
include/is/tracing.hpp
)

set(middlewares
include/is/middleware/log.hpp
include/is/middleware/metrics.hpp
include/is/middleware/tracing.hpp
)

install(FILES ${headers} DESTINATION include/is)
install(FILES ${middlewares} DESTINATION include/is/middleware)
