#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <print>
#include <string_view>
#include <vector>

#include <enet/enet.h>
#include <flatbuffers/flatbuffers.h>
#include <rpc_generated.h>

int main() {
  std::setvbuf(stdout, nullptr, _IOLBF, 0);

  enet_initialize();
  std::atexit([]{ enet_deinitialize(); });

  ENetAddress address{};
  address.host = ENET_HOST_ANY;
  address.port = 7777;

  auto* host = enet_host_create(&address, 32, 1, 0, 0);

  std::println("listening on 0.0.0.0:7777");

  for (;;) {
    ENetEvent event;
    while (enet_host_service(host, &event, 100) > 0) {
    }
  }

  enet_host_destroy(host);
  return 0;
}
