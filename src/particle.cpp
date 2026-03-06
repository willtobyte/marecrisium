#include "particle.hpp"

namespace {

constexpr float TWO_PI = 6.28318530718f;
constexpr float HALF_PI = 1.57079632679f;
constexpr float INV_HALF_PI = .63661977236f;

constexpr float SIN_C0 = .99997f;
constexpr float SIN_C1 = .16596f;
constexpr float SIN_C2 = .00759f;
constexpr float COS_C0 = .99996f;
constexpr float COS_C1 = .49985f;
constexpr float COS_C2 = .03659f;

constexpr float QUADRANT_SIGNS[8] = {1.f, 1.f, 1.f, -1.f, -1.f, -1.f, -1.f, 1.f};
constexpr int QUADRANT_MASK = 3;

std::mt19937 _engine{std::random_device{}()};

void sincos(float x, float& osin, float& ocos) noexcept {
  const auto q = static_cast<int>(x * INV_HALF_PI);
  const auto t = x - static_cast<float>(q) * HALF_PI;
  const auto t2 = t * t;

  const auto sin_t = t * (SIN_C0 - t2 * (SIN_C1 - t2 * SIN_C2));
  const auto cos_t = COS_C0 - t2 * (COS_C1 - t2 * COS_C2);

  const auto qi = (q & QUADRANT_MASK) * 2;
  const auto swap = static_cast<float>(q & 1);
  const auto keep = 1.f - swap;

  osin = (sin_t * keep + cos_t * swap) * QUADRANT_SIGNS[qi];
  ocos = (cos_t * keep + sin_t * swap) * QUADRANT_SIGNS[qi + 1];
}

int particle_index(lua_State* state) {
  const auto* emitter = *static_cast<particleemitter**>(luaL_checkudata(state, 1, "Particles"));
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "spawning") {
    lua_pushboolean(state, emitter->spawning);
    return 1;
  }

  if (key == "shown") {
    lua_pushboolean(state, emitter->shown);
    return 1;
  }

  if (key == "x") {
    lua_pushnumber(state, static_cast<lua_Number>(emitter->x));
    return 1;
  }

  if (key == "y") {
    lua_pushnumber(state, static_cast<lua_Number>(emitter->y));
    return 1;
  }

  return lua_pushnil(state), 1;
}

int particle_newindex(lua_State* state) {
  auto* emitter = *static_cast<particleemitter**>(luaL_checkudata(state, 1, "Particles"));
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "spawning") {
    emitter->spawning = lua_toboolean(state, 3) != 0;
    return 0;
  }

  if (key == "shown") {
    emitter->shown = lua_toboolean(state, 3) != 0;
    return 0;
  }

  if (key == "x") {
    emitter->x = static_cast<float>(luaL_checknumber(state, 3));
    return 0;
  }

  if (key == "y") {
    emitter->y = static_cast<float>(luaL_checknumber(state, 3));
    return 0;
  }

  if (key == "position") {
    luaL_checktype(state, 3, LUA_TTABLE);

    lua_rawgeti(state, 3, 1);
    if (!lua_isnil(state, -1)) {
      emitter->x = static_cast<float>(lua_tonumber(state, -1));
      lua_pop(state, 1);
      lua_rawgeti(state, 3, 2);
      emitter->y = static_cast<float>(lua_tonumber(state, -1));
      lua_pop(state, 1);
    } else {
      lua_pop(state, 1);
      lua_getfield(state, 3, "x");
      emitter->x = static_cast<float>(lua_tonumber(state, -1));
      lua_pop(state, 1);
      lua_getfield(state, 3, "y");
      emitter->y = static_cast<float>(lua_tonumber(state, -1));
      lua_pop(state, 1);
    }

    return 0;
  }

  return 0;
}

}

particlebatch::particlebatch(const class pixmap& pixmap, size_t count)
    : pixmap(&pixmap), count(count) {
  if (luaL_newmetatable(L, "Particles")) {
    lua_pushcfunction(L, particle_index);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, particle_newindex);
    lua_setfield(L, -2, "__newindex");
  }
  lua_pop(L, 1);

  emitter.hw = static_cast<float>(pixmap.width()) * .5f;
  emitter.hh = static_cast<float>(pixmap.height()) * .5f;

  x.resize(count);
  y.resize(count);
  vx.resize(count);
  vy.resize(count);
  gx.resize(count);
  gy.resize(count);
  life.resize(count);
  scale.resize(count);
  angle.resize(count);
  av.resize(count);
  af.resize(count);
  vertices.resize(count * 4);
  indices.resize(count * 6);
  respawn.resize(count);

  for (auto i = 0uz; i < count; ++i) {
    const auto base = static_cast<int>(i * 4);
    const auto offset = i * 6uz;
    indices[offset] = base;
    indices[offset + 1] = base + 1;
    indices[offset + 2] = base + 2;
    indices[offset + 3] = base;
    indices[offset + 4] = base + 2;
    indices[offset + 5] = base + 3;
  }
}

std::pair<float, float> particle::range(lua_State* state) noexcept {
  float start = .0f;
  float end = .0f;

  if (lua_istable(state, -1)) {
    lua_getfield(state, -1, "start");
    start = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);

    lua_getfield(state, -1, "end");
    end = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);
  }

  return {start, end};
}

void particle::update(particlebatch& batch, float delta) {
  auto& emitter = batch.emitter;
  const auto n = batch.count;

  auto* __restrict xs = batch.x.data();
  auto* __restrict ys = batch.y.data();
  auto* __restrict vxs = batch.vx.data();
  auto* __restrict vys = batch.vy.data();
  auto* __restrict gxs = batch.gx.data();
  auto* __restrict gys = batch.gy.data();
  auto* __restrict lifes = batch.life.data();
  auto* __restrict scales = batch.scale.data();
  auto* __restrict angles = batch.angle.data();
  auto* __restrict avs = batch.av.data();
  auto* __restrict afs = batch.af.data();

  for (auto i = 0uz; i < n; ++i) {
    lifes[i] -= delta;
    avs[i] += afs[i] * delta;
    angles[i] += avs[i] * delta;
    angles[i] -= TWO_PI * static_cast<float>(angles[i] >= TWO_PI);
    angles[i] += TWO_PI * static_cast<float>(angles[i] < .0f);
    vxs[i] += gxs[i] * delta;
    vys[i] += gys[i] * delta;
    xs[i] += vxs[i] * delta;
    ys[i] += vys[i] * delta;
  }

  if (emitter.spawning) {
    const auto px = emitter.x;
    const auto py = emitter.y;

    auto* __restrict respawning = batch.respawn.data();
    auto dead = 0uz;

    for (auto i = 0uz; i < n; ++i) {
      respawning[dead] = i;
      dead += static_cast<size_t>(lifes[i] <= .0f);
    }

    for (auto j = 0uz; j < dead; ++j) {
      const auto i = respawning[j];
      const auto radius = emitter.radiusd(_engine);
      const auto spawnangle = emitter.angled(_engine);

      float sa, ca;
      sincos(spawnangle, sa, ca);

      xs[i] = px + emitter.xspawnd(_engine) + radius * ca;
      ys[i] = py + emitter.yspawnd(_engine) + radius * sa;
      vxs[i] = emitter.xveld(_engine);
      vys[i] = emitter.yveld(_engine);
      gxs[i] = emitter.gxd(_engine);
      gys[i] = emitter.gyd(_engine);
      avs[i] = emitter.rotveld(_engine);
      afs[i] = emitter.rotforced(_engine);
      lifes[i] = emitter.lifed(_engine);
      scales[i] = emitter.scaled(_engine);
      angles[i] = spawnangle;
    }
  }

  const auto hw = emitter.hw;
  const auto hh = emitter.hh;
  auto* verts = batch.vertices.data();

  for (auto i = 0uz; i < n; ++i) {
    const auto remaining = lifes[i];
    auto* quad = verts + i * 4;

    if (remaining <= .0f) {
      const SDL_FColor color = {1.f, 1.f, 1.f, .0f};
      quad[0] = {{.0f, .0f}, color, {.0f, .0f}};
      quad[1] = {{.0f, .0f}, color, {1.f, .0f}};
      quad[2] = {{.0f, .0f}, color, {1.f, 1.f}};
      quad[3] = {{.0f, .0f}, color, {.0f, 1.f}};

      continue;
    }

    const auto alpha = std::min(remaining, 1.f);

    const auto s = scales[i];
    const auto shw = hw * s;
    const auto shh = hh * s;

    float sa, ca;
    sincos(angles[i], sa, ca);

    const auto px = xs[i];
    const auto py = ys[i];
    const SDL_FColor color = {1.f, 1.f, 1.f, alpha};

    const auto dx0 = -shw * ca + shh * sa;
    const auto dy0 = -shw * sa - shh * ca;
    const auto dx1 = shw * ca + shh * sa;
    const auto dy1 = shw * sa - shh * ca;

    quad[0] = {{px + dx0, py + dy0}, color, {.0f, .0f}};
    quad[1] = {{px + dx1, py + dy1}, color, {1.f, .0f}};
    quad[2] = {{px - dx0, py - dy0}, color, {1.f, 1.f}};
    quad[3] = {{px - dx1, py - dy1}, color, {.0f, 1.f}};
  }
}

void particle::draw(const particlebatch& batch) noexcept {
  SDL_RenderGeometry(renderer,
    static_cast<SDL_Texture*>(*batch.pixmap),
    batch.vertices.data(),
    static_cast<int>(batch.vertices.size()),
    batch.indices.data(),
    static_cast<int>(batch.indices.size()));
}
