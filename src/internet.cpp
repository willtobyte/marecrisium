namespace {
  static ENetHost *host = nullptr;
  static ENetPeer *peer = nullptr;
  static int connect_ref = LUA_NOREF;
  static int disconnect_ref = LUA_NOREF;
  static bool connected = false;

  static void release() noexcept {
    if (peer) {
      enet_peer_reset(peer);
      peer = nullptr;
    }

    connected = false;
  }

  static void fire(int &reference, bool argument, bool include) noexcept {
    if (reference == LUA_NOREF) [[likely]] return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, reference);
    luaL_unref(L, LUA_REGISTRYINDEX, reference);
    reference = LUA_NOREF;
    int arity = 0;
    if (include) { lua_pushboolean(L, argument ? 1 : 0); arity = 1; }
    pcall(L, arity, 0, fault::ignore);
  }

  static int connect(lua_State *state) {
    if (connected || peer) {
      lua_pushboolean(state, 0);
      return 1;
    }

    const auto *target = luaL_checkstring(state, 1);
    const auto port = static_cast<enet_uint16>(luaL_checkinteger(state, 2));
    const auto has_callback = lua_isfunction(state, 3);

    if (!host) {
      host = enet_host_create(nullptr, 1, 1, 0, 0);
      if (!host) [[unlikely]] {
        lua_pushboolean(state, 0);
        return 1;
      }

      std::atexit([]{
        release();

        if (host) {
          enet_host_destroy(host);
          host = nullptr;
        }
      });
    }

    ENetAddress address{};
    if (enet_address_set_host(&address, target) != 0) [[unlikely]] {
      lua_pushboolean(state, 0);
      return 1;
    }
    address.port = port;

    peer = enet_host_connect(host, &address, 1, 0);
    if (!peer) [[unlikely]] {
      lua_pushboolean(state, 0);
      return 1;
    }

    if (connect_ref != LUA_NOREF) {
      luaL_unref(state, LUA_REGISTRYINDEX, connect_ref);
      connect_ref = LUA_NOREF;
    }
    if (has_callback) {
      lua_pushvalue(state, 3);
      connect_ref = luaL_ref(state, LUA_REGISTRYINDEX);
    }

    lua_pushboolean(state, 1);
    return 1;
  }

  static int disconnect(lua_State *state) {
    if (!peer) {
      lua_pushboolean(state, 0);
      return 1;
    }

    if (lua_isfunction(state, 1)) {
      if (disconnect_ref != LUA_NOREF) luaL_unref(state, LUA_REGISTRYINDEX, disconnect_ref);
      lua_pushvalue(state, 1);
      disconnect_ref = luaL_ref(state, LUA_REGISTRYINDEX);
    }

    enet_peer_disconnect(peer, 0);
    lua_pushboolean(state, 1);
    return 1;
  }

  static int openurl(lua_State *state) {
    const auto *url = luaL_checkstring(state, 1);
    lua_pushboolean(state, SDL_OpenURL(url) ? 1 : 0);
    return 1;
  }
}

void internet::wire() {
  lua_newtable(L);

  lua_pushcfunction(L, connect);
  lua_setfield(L, -2, "connect");

  lua_pushcfunction(L, disconnect);
  lua_setfield(L, -2, "disconnect");

  lua_setglobal(L, "internet");

  lua_pushcfunction(L, openurl);
  lua_setglobal(L, "openurl");
}

void internet::tick() {
  if (!host) return;

  ENetEvent event;
  while (enet_host_service(host, &event, 0) > 0) {
    switch (event.type) {
      case ENET_EVENT_TYPE_CONNECT:
        connected = true;
        fire(connect_ref, true, true);
        break;

      case ENET_EVENT_TYPE_DISCONNECT: {
        const auto wasconnected = connected;
        peer = nullptr;
        connected = false;
        if (wasconnected) {
          fire(disconnect_ref, false, false);
        } else {
          fire(connect_ref, false, true);
          if (disconnect_ref != LUA_NOREF) {
            luaL_unref(L, LUA_REGISTRYINDEX, disconnect_ref);
            disconnect_ref = LUA_NOREF;
          }
        }
        break;
      }

      case ENET_EVENT_TYPE_RECEIVE:
        enet_packet_destroy(event.packet);
        break;

      case ENET_EVENT_TYPE_NONE:
        break;
    }
  }
}
