cmake_minimum_required(VERSION 4.0)
project(server LANGUAGES C CXX)

set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)

find_package(enet CONFIG REQUIRED)

find_package(unordered_dense CONFIG REQUIRED)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../schemas ${CMAKE_BINARY_DIR}/schemas)

add_executable(server main.cpp)

target_link_libraries(server PRIVATE
  enet::enet
  unordered_dense::unordered_dense
  rpc
)
