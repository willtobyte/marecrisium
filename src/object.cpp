namespace {
  int object_index(lua_State* state) {
    const auto* proxy = static_cast<objectproxy*>(luaL_checkudata(state, 1, "Object"));
    const std::string_view key = luaL_checkstring(state, 2);

    if (!proxy->registry->valid(proxy->entity))
      return lua_pushnil(state), 1;

    auto& registry = *proxy->registry;
    const auto entity = proxy->entity;

    if (key == "x") {
      lua_pushnumber(state, static_cast<double>(registry.get<transform>(entity).x));
      return 1;
    }

    if (key == "y") {
      lua_pushnumber(state, static_cast<double>(registry.get<transform>(entity).y));
      return 1;
    }

    if (key == "scale") {
      lua_pushnumber(state, static_cast<double>(registry.get<transform>(entity).scale));
      return 1;
    }

    if (key == "angle") {
      lua_pushnumber(state, static_cast<double>(registry.get<transform>(entity).angle));
      return 1;
    }

    if (key == "alpha") {
      lua_pushnumber(state, static_cast<double>(registry.get<transform>(entity).alpha));
      return 1;
    }

    if (key == "shown") {
      lua_pushboolean(state, registry.get<transform>(entity).shown);
      return 1;
    }

    if (key == "animation") {
      if (registry.all_of<animation>(entity)) {
        const auto& a = registry.get<animation>(entity);
        if (a.playing && a.clip_count > 0) {
          const auto* strings = registry.ctx().get<stringpool*>();
          lua_pushstring(state, strings->get(a.clips[a.active].name));
          return 1;
        }
      }
      return lua_pushnil(state), 1;
    }

    assert(proxy->object_reference != LUA_NOREF && "object must have an object reference");

    lua_rawgeti(state, LUA_REGISTRYINDEX, proxy->object_reference);
    lua_getfield(state, -1, key.data());
    lua_remove(state, -2);
    if (!lua_isnil(state, -1))
      return 1;
    lua_pop(state, 1);

    return lua_pushnil(state), 1;
  }

  int object_newindex(lua_State* state) {
    auto* proxy = static_cast<objectproxy*>(luaL_checkudata(state, 1, "Object"));
    const std::string_view key = luaL_checkstring(state, 2);

    if (!proxy->registry->valid(proxy->entity))
      return 0;

    auto& registry = *proxy->registry;
    const auto entity = proxy->entity;

    if (key == "x") {
      registry.get<transform>(entity).x = static_cast<float>(luaL_checknumber(state, 3));
      return 0;
    }

    if (key == "y") {
      registry.get<transform>(entity).y = static_cast<float>(luaL_checknumber(state, 3));
      return 0;
    }

    if (key == "scale") {
      registry.get<transform>(entity).scale = static_cast<float>(luaL_checknumber(state, 3));
      return 0;
    }

    if (key == "angle") {
      registry.get<transform>(entity).angle = static_cast<float>(luaL_checknumber(state, 3));
      return 0;
    }

    if (key == "alpha") {
      registry.get<transform>(entity).alpha = static_cast<uint8_t>(luaL_checknumber(state, 3));
      return 0;
    }

    if (key == "shown") {
      registry.get<transform>(entity).shown = lua_toboolean(state, 3) != 0;
      return 0;
    }

    if (key == "animation") {
      if (registry.all_of<animation>(entity)) {
        const std::string_view value = luaL_checkstring(state, 3);
        const auto hash = entt::hashed_string{value.data()}.value();

        auto& a = registry.get<animation>(entity);

        for (uint8_t i = 0; i < a.clip_count; ++i) {
          if (a.clips[i].name == hash) {
            const auto previous = a.playing ? a.clips[a.active].name : entt::id_type{};
            a.active = i;
            a.current = 0;
            a.elapsed = 0.0f;
            a.playing = true;

            if (proxy->object_reference != LUA_NOREF && proxy->self_reference != LUA_NOREF) {
              const auto* strings = registry.ctx().get<stringpool*>();

              if (previous != 0 && previous != hash) {
                lua_rawgeti(state, LUA_REGISTRYINDEX, proxy->object_reference);
                lua_getfield(state, -1, "on_animation_end");
                if (lua_isfunction(state, -1)) {
                  lua_rawgeti(state, LUA_REGISTRYINDEX, proxy->self_reference);
                  lua_pushstring(state, strings->get(previous));
                  if (lua_pcall(state, 2, 0, 0) != 0) [[unlikely]] {
                    std::string error = lua_tostring(state, -1);
                    lua_pop(state, 1);
                    throw std::runtime_error(error);
                  }
                } else {
                  lua_pop(state, 1);
                }
                lua_pop(state, 1);
              }

              lua_rawgeti(state, LUA_REGISTRYINDEX, proxy->object_reference);
              lua_getfield(state, -1, "on_animation_begin");
              if (lua_isfunction(state, -1)) {
                lua_rawgeti(state, LUA_REGISTRYINDEX, proxy->self_reference);
                lua_pushstring(state, strings->get(hash));
                if (lua_pcall(state, 2, 0, 0) != 0) [[unlikely]] {
                    std::string error = lua_tostring(state, -1);
                    lua_pop(state, 1);
                    throw std::runtime_error(error);
                  }
              } else {
                lua_pop(state, 1);
              }
              lua_pop(state, 1);
            }

            return 0;
          }
        }
      }
      return 0;
    }

    assert(proxy->object_reference != LUA_NOREF && "object must have an object reference");

    lua_rawgeti(state, LUA_REGISTRYINDEX, proxy->object_reference);
    lua_pushvalue(state, 3);
    lua_setfield(state, -2, key.data());
    lua_pop(state, 1);
    return 0;
  }

  int object_gc(lua_State* state) {
    auto* proxy = static_cast<objectproxy*>(luaL_checkudata(state, 1, "Object"));
    if (proxy->object_reference != LUA_NOREF)
      luaL_unref(state, LUA_REGISTRYINDEX, proxy->object_reference);

    proxy->~objectproxy();
    return 0;
  }

}

void objectproxy::on_destroy(entt::registry& registry, entt::entity entity) {
  auto& proxy = registry.get<objectproxy>(entity);

  if (proxy.self_reference != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.self_reference);
    auto* ud = static_cast<objectproxy*>(lua_touserdata(L, -1));
    if (ud && ud->object_reference != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, ud->object_reference);
      ud->object_reference = LUA_NOREF;
    }
    lua_pop(L, 1);
    luaL_unref(L, LUA_REGISTRYINDEX, proxy.self_reference);
  }
}

objectproxy::objectproxy(entt::registry& registry, entt::entity entity, std::string_view stage, std::string_view name, int environment_reference)
    : registry(&registry), entity(entity) {
  if (luaL_newmetatable(L, "Object")) {
    lua_pushcfunction(L, object_index);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, object_newindex);
    lua_setfield(L, -2, "__newindex");

    lua_pushcfunction(L, object_gc);
    lua_setfield(L, -2, "__gc");
  }
  lua_pop(L, 1);

  const auto filename = std::format("objects/{}/{}.lua", stage, name);
  const auto buffer = io::read(filename);
  const auto* data = reinterpret_cast<const char*>(buffer.data());
  const auto size = buffer.size();
  const auto label = std::format("@{}", filename);

  if (luaL_loadbuffer(L, data, size, label.c_str()) != 0) [[unlikely]] {
    std::string error = lua_tostring(L, -1);
    lua_pop(L, 1);
    throw std::runtime_error(error);
  }

  if (environment_reference != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, environment_reference);
    lua_setfenv(L, -2);
  }

  if (lua_pcall(L, 0, 1, 0) != 0) [[unlikely]] {
    std::string error = lua_tostring(L, -1);
    lua_pop(L, 1);
    throw std::runtime_error(error);
  }

  object_reference = luaL_ref(L, LUA_REGISTRYINDEX);

  auto* memory = lua_newuserdata(L, sizeof(objectproxy));
  std::memcpy(memory, this, sizeof(objectproxy));
  luaL_getmetatable(L, "Object");
  lua_setmetatable(L, -2);

  self_reference = luaL_ref(L, LUA_REGISTRYINDEX);
}
