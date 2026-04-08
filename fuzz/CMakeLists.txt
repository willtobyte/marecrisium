cmake_minimum_required(VERSION 3.15)

if(NOT CMAKE_PROJECT_NAME)
  project(fuzzer LANGUAGES CXX)

  set(CMAKE_CXX_EXTENSIONS ON)
  set(CMAKE_CXX_STANDARD 23)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

find_package(PhysFS CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(unordered_dense CONFIG REQUIRED)

add_executable(fuzzer cartridge.cpp)
target_include_directories(fuzzer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(fuzzer PRIVATE
  physfs-static
  zstd::libzstd_static
  unordered_dense::unordered_dense
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(fuzzer PRIVATE
    -fsanitize=fuzzer,address,undefined
    -fno-omit-frame-pointer -g -O1
  )
  target_link_options(fuzzer PRIVATE
    -fsanitize=fuzzer,address,undefined
  )
elseif(MSVC)
  target_compile_definitions(fuzzer PRIVATE FUZZ_CORPUS_RUNNER)
endif()
