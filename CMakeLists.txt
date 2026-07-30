cmake_minimum_required(VERSION 4.0)
project(carimbo LANGUAGES C CXX)

set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

set(SDL3_DIR "${CMAKE_BINARY_DIR}")

file(GLOB SOURCE_FILES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB HEADER_FILES CONFIGURE_DEPENDS "src/*.hpp")

add_compile_definitions(
  $<$<CONFIG:Debug>:DEBUG>
  $<$<PLATFORM_ID:Windows>:NOMINMAX>
  $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
)

set_source_files_properties(
  src/miniaudio.cpp
  src/stb.cpp
  PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set_property(SOURCE src/stb.cpp APPEND PROPERTY COMPILE_OPTIONS
    -fno-lto
    -Wno-error=maybe-uninitialized
  )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set_property(SOURCE src/stb.cpp APPEND PROPERTY COMPILE_OPTIONS
    -fno-sanitize=implicit-conversion
  )
endif()

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
  ${SOURCE_FILES}
  ${HEADER_FILES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
  WIN32_EXECUTABLE $<$<PLATFORM_ID:Windows>:$<NOT:$<CONFIG:Debug>>>
)

find_package(box2d CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(stb CONFIG REQUIRED)
find_package(luajit CONFIG REQUIRED)
find_package(miniaudio CONFIG REQUIRED)
find_package(SQLite3 CONFIG REQUIRED)
find_package(yyjson CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(mimalloc CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
  $<$<NOT:$<CONFIG:Debug>>:mimalloc-static>
  box2d::box2d
  EnTT::EnTT
  SDL3::SDL3-static
  stb::stb
  miniaudio::miniaudio
  luajit::luajit
  SQLite::SQLite3
  yyjson::yyjson
  zstd::libzstd_static
)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/common.hpp)
