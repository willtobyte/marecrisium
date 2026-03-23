#include "particlepool.hpp"

[[nodiscard]] static std::pair<float, float> read_range(lua_State* state, const char* field) noexcept {
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

config& particlepool::get(std::string_view kind) {
  const auto key = entt::hashed_string{kind.data()}.value();
  const auto it = _pool.find(key);
  if (it != _pool.end()) [[likely]]
    return it->second;

  config config{};

  const auto filename = std::format("particles/{}.lua", kind);
  const auto buffer = io::read(filename);
  const auto label = std::format("@{}", filename);
  compile(L, buffer, label);

  pcall(L, 0, 1);

  lua_getfield(L, -1, "count");
  config.count = lua_isnumber(L, -1) ? static_cast<size_t>(lua_tonumber(L, -1)) : 0uz;
  lua_pop(L, 1);

  lua_getfield(L, -1, "spawn");
  if (lua_istable(L, -1)) {
    config.xspawn = read_range(L, "x");
    config.yspawn = read_range(L, "y");
    config.radius = read_range(L, "radius");
    config.angle = read_range(L, "angle");
    config.scale = read_range(L, "scale");
    config.life = read_range(L, "life");
  }
  lua_pop(L, 1);

  lua_getfield(L, -1, "velocity");
  if (lua_istable(L, -1)) {
    config.xvel = read_range(L, "x");
    config.yvel = read_range(L, "y");
  }
  lua_pop(L, 1);

  lua_getfield(L, -1, "gravity");
  if (lua_istable(L, -1)) {
    config.gx = read_range(L, "x");
    config.gy = read_range(L, "y");
  }
  lua_pop(L, 1);

  lua_getfield(L, -1, "rotation");
  if (lua_istable(L, -1)) {
    config.rforce = read_range(L, "force");
    config.rvel = read_range(L, "velocity");
  }
  lua_pop(L, 1);

  lua_pop(L, 1);

  return _pool.try_emplace(key, std::move(config)).first->second;
}

void particlepool::clear() {
  _pool.clear();
}
