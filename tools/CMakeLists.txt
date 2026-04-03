cmake_minimum_required(VERSION 3.15)

if(NOT CMAKE_PROJECT_NAME)
  project(tools LANGUAGES CXX)

  set(CMAKE_CXX_EXTENSIONS ON)
  set(CMAKE_CXX_STANDARD 23)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

find_package(zstd CONFIG REQUIRED)

add_executable(pack pack.cpp)
target_link_libraries(pack PRIVATE zstd::libzstd_static)

add_executable(unpack unpack.cpp)
target_link_libraries(unpack PRIVATE zstd::libzstd_static)
