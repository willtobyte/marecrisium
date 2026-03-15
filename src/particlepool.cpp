#include "particlepool.hpp"

[[nodiscard]] static std::pair<float, float> read_range(lua_State* state, const char* field) noexcept {
  float a = .0f, b = .0f;
  lua_getfield(state, -1, field);
  if (lua_istable(state, -1)) {
    lua_rawgeti(state, -1, 1);
    a = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);

    lua_rawgeti(state, -1, 2);
    b = static_cast<float>(lua_tonumber(state, -1));
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
  const auto* data = reinterpret_cast<const char*>(buffer.data());
  const auto size = buffer.size();
  const auto label = std::format("@{}", filename);

  if (luaL_loadbuffer(L, data, size, label.c_str()) != 0) [[unlikely]] {
    std::string error{lua_tostring(L, -1)};
    lua_pop(L, 1);
    throw std::runtime_error{std::move(error)};
  }

  if (lua_pcall(L, 0, 1, 0) != 0) [[unlikely]] {
    std::string error{lua_tostring(L, -1)};
    lua_pop(L, 1);
    throw std::runtime_error{std::move(error)};
  }

  lua_getfield(L, -1, "count");
  config.count = static_cast<size_t>(lua_tonumber(L, -1));
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
