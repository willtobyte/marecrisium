cmake_minimum_required(VERSION 3.15)
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
  PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
  ${SOURCE_FILES}
  ${HEADER_FILES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
  WIN32_EXECUTABLE $<$<PLATFORM_ID:Windows>:$<NOT:$<CONFIG:Debug>>>
)

find_package(box2d CONFIG REQUIRED)
find_package(enet CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(PhysFS CONFIG REQUIRED)
# find_package(protobuf CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(sentry CONFIG REQUIRED)
find_package(SPNG CONFIG REQUIRED)
find_package(luajit CONFIG REQUIRED)
find_package(miniaudio CONFIG REQUIRED)
find_package(opusfile CONFIG REQUIRED)
find_package(SQLite3 CONFIG REQUIRED)
find_package(unordered_dense CONFIG REQUIRED)
find_package(yyjson CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
  box2d::box2d
  enet::enet
  EnTT::EnTT
  physfs-static
  # protobuf::libprotobuf
  SDL3::SDL3-static
  sentry-native::sentry-native
  spng::spng_static
  miniaudio::miniaudio
  opusfile::opusfile
  luajit::luajit
  SQLite::SQLite3
  unordered_dense::unordered_dense
  yyjson::yyjson
  zstd::libzstd_static
)

target_precompile_headers(${PROJECT_NAME} PRIVATE ${HEADER_FILES})
