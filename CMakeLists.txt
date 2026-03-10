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
  $<$<BOOL:${DEVELOPMENT}>:DEVELOPMENT>
  $<$<PLATFORM_ID:Windows>:NOMINMAX>
  $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
  $<IF:$<CXX_COMPILER_ID:MSVC>,RESTRICT=__restrict,RESTRICT=__restrict__>
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
find_package(EnTT CONFIG REQUIRED)
find_package(libwebsockets CONFIG REQUIRED)
find_package(PhysFS CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(SPNG CONFIG REQUIRED)
find_package(luajit CONFIG REQUIRED)
find_package(miniaudio CONFIG REQUIRED)
find_package(opusfile CONFIG REQUIRED)
find_package(SQLite3 CONFIG REQUIRED)
find_package(yyjson CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
  box2d::box2d
  EnTT::EnTT
  physfs-static
  SDL3::SDL3-static
  spng::spng_static
  miniaudio::miniaudio
  opusfile::opusfile
  luajit::luajit
  websockets
  SQLite::SQLite3
  yyjson::yyjson
)

target_precompile_headers(${PROJECT_NAME} PRIVATE ${HEADER_FILES})
