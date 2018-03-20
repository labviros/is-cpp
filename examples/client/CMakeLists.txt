cmake_minimum_required(VERSION 3.0)

add_executable(client client.cpp)
target_link_libraries(client PRIVATE is myprotos)
target_include_directories(client PRIVATE ${PROJECT_SOURCE_DIR}) # fixme
