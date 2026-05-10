#include <cstdio>
#include <cstdlib>
#include <print>

#include <enet/enet.h>
#include <flatbuffers/flatbuffers.h>
#include <rpc_generated.h>

int main() {
  std::setvbuf(stdout, nullptr, _IONBF, 0);

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
      switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT:
          std::println("connect from {}.{}.{}.{}:{}",
            (event.peer->address.host)       & 0xffu,
            (event.peer->address.host >>  8) & 0xffu,
            (event.peer->address.host >> 16) & 0xffu,
            (event.peer->address.host >> 24) & 0xffu,
            event.peer->address.port);
          break;

        case ENET_EVENT_TYPE_DISCONNECT:
          std::println("disconnect");
          event.peer->data = nullptr;
          break;

        case ENET_EVENT_TYPE_RECEIVE:
          std::println("receive {} bytes on channel {}",
            event.packet->dataLength,
            static_cast<unsigned>(event.channelID));
          enet_packet_destroy(event.packet);
          break;

        case ENET_EVENT_TYPE_NONE:
          break;
      }
    }
  }

  enet_host_destroy(host);
  return 0;
}
