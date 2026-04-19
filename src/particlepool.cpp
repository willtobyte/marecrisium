#include "particlepool.hpp"

static std::pair<float, float> read_range(lua_State* state, const char* field) {
  float a = .0f, b = .0f;
  lua_getfield(state, -1, field);
  if (lua_istable(state, -1)) {
    lua_rawgeti(state, -1, 1);
    a = lua_isnumber(state, -1) ? static_cast<float>(lua_tonumber(state, -1)) : .0f;
    lua_pop(state, 1);

    lua_rawgeti(state, -1, 2);
    b = lua_isnumber(state, -1) ? static_cast<float>(lua_tonumber(state, -1)) : .0f;
    lua_pop(state, 1);
  }
  lua_pop(state, 1);

  return {a, b};
}

config* particlepool::get(std::string_view kind) {
  const auto key = entt::hashed_string{kind.data()};
  const auto [it, inserted] = _pool.try_emplace(key, nullptr);
  if (inserted) [[unlikely]] {
    auto config = std::make_unique<struct config>();

    const auto filename = std::format("particles/{}.lua", kind);
    const auto buffer = io::read(filename);
    const auto chunk = std::format("@{}", filename);
    compile(L, buffer, chunk);

    pcall(L, 0, 1);

    lua_getfield(L, -1, "count");
    config->count = lua_isnumber(L, -1) ? static_cast<size_t>(lua_tonumber(L, -1)) : 0uz;
    lua_pop(L, 1);

    lua_getfield(L, -1, "spawn");
    if (lua_istable(L, -1)) {
      config->spawn_x = read_range(L, "x");
      config->spawn_y = read_range(L, "y");
      config->radius = read_range(L, "radius");
      config->angle = read_range(L, "angle");
      config->scale = read_range(L, "scale");
      config->life = read_range(L, "life");
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "velocity");
    if (lua_istable(L, -1)) {
      config->velocity_x = read_range(L, "x");
      config->velocity_y = read_range(L, "y");
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "gravity");
    if (lua_istable(L, -1)) {
      config->gravity_x = read_range(L, "x");
      config->gravity_y = read_range(L, "y");
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "rotation");
    if (lua_istable(L, -1)) {
      config->rotation_force = read_range(L, "force");
      config->rotation_velocity = read_range(L, "velocity");
    }
    lua_pop(L, 1);

    lua_pop(L, 1);

    it->second = std::move(config);
  }

  return it->second.get();
}

void particlepool::clear() {
  _pool.clear();
}
