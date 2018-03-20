cmake_minimum_required(VERSION 3.0)

add_executable(hello hello.cpp)
target_link_libraries(hello PRIVATE is myprotos)
target_include_directories(hello PRIVATE ${PROJECT_SOURCE_DIR}) # fixme