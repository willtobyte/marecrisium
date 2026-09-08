static std::pair<float, float> read_range(lua_State* state, const char* field) {
  float minimum = .0f, maximum = .0f;

  lua_getfield(state, -1, field);
  if (lua_istable(state, -1)) {
    lua_rawgeti(state, -1, 1);
    lua_rawgeti(state, -2, 2);

    minimum = static_cast<float>(lua_tonumber(state, -2));
    maximum = static_cast<float>(lua_tonumber(state, -1));

    lua_pop(state, 2);
  }

  lua_pop(state, 1);

  return {minimum, maximum};
}

config* particlepool::get(std::string_view kind) {
  if (const auto it = _pool.find(kind); it != _pool.end()) [[likely]]
    return &it->second;

  struct config instance;
  const auto chunk = std::format("@particles/{}.lua", kind);
  const auto filename = std::string_view{chunk}.substr(1);
  const auto source = io::read(filename);
  if (luaL_loadbuffer(L, reinterpret_cast<const char*>(source.data()), source.size(), chunk.c_str()) != LUA_OK) [[unlikely]]
    throw std::runtime_error{lua_tostring(L, -1)};

  if (pcall(L, 0, 1) != LUA_OK) [[unlikely]]
    throw std::runtime_error{lua_tostring(L, -1)};

  lua_getfield(L, -1, "count");
  instance.count = static_cast<size_t>(lua_tonumber(L, -1));
  lua_pop(L, 1);

  lua_getfield(L, -1, "spawn");
  if (lua_istable(L, -1)) {
    instance.spawn.x = read_range(L, "x");
    instance.spawn.y = read_range(L, "y");
    instance.radius = read_range(L, "radius");
    instance.angle = read_range(L, "angle");
    instance.scale = read_range(L, "scale");
    instance.life = read_range(L, "life");
  }

  lua_pop(L, 1);

  lua_getfield(L, -1, "velocity");
  if (lua_istable(L, -1)) {
    instance.velocity.x = read_range(L, "x");
    instance.velocity.y = read_range(L, "y");
  }

  lua_pop(L, 1);

  lua_getfield(L, -1, "gravity");
  if (lua_istable(L, -1)) {
    instance.gravity.x = read_range(L, "x");
    instance.gravity.y = read_range(L, "y");
  }

  lua_pop(L, 1);

  lua_getfield(L, -1, "rotation");
  if (lua_istable(L, -1)) {
    instance.rotation.force = read_range(L, "force");
    instance.rotation.velocity = read_range(L, "velocity");
  }

  lua_pop(L, 1);
  lua_pop(L, 1);

  auto* result = &_pool.emplace(kind, std::move(instance)).first->second;

  return result;
}

void particlepool::clear() {
  _pool.clear();
}
