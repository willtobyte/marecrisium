find_package(flatbuffers CONFIG REQUIRED)

set(GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${GENERATED}")

set(SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/rpc.fbs")

add_custom_command(
  OUTPUT  "${GENERATED}/rpc_generated.h"
  COMMAND $<TARGET_FILE:flatbuffers::flatc> --cpp -o "${GENERATED}" "${SCHEMA}"
  DEPENDS "${SCHEMA}" flatbuffers::flatc
  VERBATIM
)

add_custom_target(codegen DEPENDS "${GENERATED}/rpc_generated.h")

add_library(rpc INTERFACE)
add_dependencies(rpc codegen)
target_include_directories(rpc INTERFACE "${GENERATED}")
target_link_libraries(rpc INTERFACE flatbuffers::flatbuffers)
