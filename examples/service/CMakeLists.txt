cmake_minimum_required(VERSION 3.0)

add_executable(service service.cpp)
target_compile_features(service PUBLIC cxx_generic_lambdas)
target_link_libraries(service PRIVATE is myprotos)
target_include_directories(service PRIVATE ${PROJECT_SOURCE_DIR}) # fixme
