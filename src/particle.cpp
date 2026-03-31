#include "particle.hpp"

namespace {

constexpr float TWO_PI = 6.28318530718f;

namespace property {
  using entt::operator""_hs;

  constexpr auto active   = "active"_hs.value();
  constexpr auto x        = "x"_hs.value();
  constexpr auto y        = "y"_hs.value();
  constexpr auto position = "position"_hs.value();
  constexpr auto sound    = "sound"_hs.value();
}

int particle_index(lua_State* state) {
  const auto* self = *static_cast<particle**>(luaL_checkudata(state, 1, "Particle"));
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)}.value();

  switch (id) {
    case property::active:
      lua_pushboolean(state, self->active() ? 1 : 0);
      return 1;

    case property::x:
      lua_pushnumber(state, static_cast<lua_Number>(self->x()));
      return 1;

    case property::y:
      lua_pushnumber(state, static_cast<lua_Number>(self->y()));
      return 1;

    case property::position:
      lua_createtable(state, 2, 0);
      lua_pushnumber(state, static_cast<lua_Number>(self->x()));
      lua_rawseti(state, -2, 1);
      lua_pushnumber(state, static_cast<lua_Number>(self->y()));
      lua_rawseti(state, -2, 2);
      return 1;

    case property::sound: {
      auto* fx = self->sound();
      if (!fx)
        return lua_pushnil(state), 1;

      auto **m = static_cast<class sound**>(lua_newuserdata(state, sizeof(class sound*)));
      *m = fx;
      luaL_getmetatable(state, "Sound");
      lua_setmetatable(state, -2);

      return 1;
    }

    default:
      return lua_pushnil(state), 1;
  }
}

int particle_newindex(lua_State* state) {
  auto* self = *static_cast<particle**>(luaL_checkudata(state, 1, "Particle"));
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)}.value();

  switch (id) {
    case property::active:
      self->set_active(lua_toboolean(state, 3) != 0);
      return 0;

    case property::x:
      self->set_x(static_cast<float>(luaL_checknumber(state, 3)));
      return 0;

    case property::y:
      self->set_y(static_cast<float>(luaL_checknumber(state, 3)));
      return 0;

    case property::position: {
      luaL_checktype(state, 3, LUA_TTABLE);
      lua_rawgeti(state, 3, 1);
      lua_rawgeti(state, 3, 2);
      const auto px = static_cast<float>(lua_tonumber(state, -2));
      const auto py = static_cast<float>(lua_tonumber(state, -1));
      lua_pop(state, 2);
      self->set_x(px);
      self->set_y(py);
      return 0;
    }

    default:
      return 0;
  }
}
}

std::mt19937& particle::rng() noexcept {
  static std::mt19937 engine{std::random_device{}()};
  return engine;
}

particle::particle(const config& config, const pixmap& texture, float x, float y, bool active)
    : _x(x)
    , _y(y)
    , _hw(static_cast<float>(texture.width()) * .5f)
    , _hh(static_cast<float>(texture.height()) * .5f)
    , _active(active)
    , _count(config.count)
    , _texture(&texture)
    , _xspawnd(std::min(config.xspawn.first, config.xspawn.second), std::max(config.xspawn.first, config.xspawn.second))
    , _yspawnd(std::min(config.yspawn.first, config.yspawn.second), std::max(config.yspawn.first, config.yspawn.second))
    , _radiusd(std::min(config.radius.first, config.radius.second), std::max(config.radius.first, config.radius.second))
    , _angled(std::min(config.angle.first, config.angle.second), std::max(config.angle.first, config.angle.second))
    , _xveld(std::min(config.xvel.first, config.xvel.second), std::max(config.xvel.first, config.xvel.second))
    , _yveld(std::min(config.yvel.first, config.yvel.second), std::max(config.yvel.first, config.yvel.second))
    , _gxd(std::min(config.gx.first, config.gx.second), std::max(config.gx.first, config.gx.second))
    , _gyd(std::min(config.gy.first, config.gy.second), std::max(config.gy.first, config.gy.second))
    , _scaled(std::min(config.scale.first, config.scale.second), std::max(config.scale.first, config.scale.second))
    , _lifed(std::min(config.life.first, config.life.second), std::max(config.life.first, config.life.second))
    , _rotforced(std::min(config.rforce.first, config.rforce.second), std::max(config.rforce.first, config.rforce.second))
    , _rotveld(std::min(config.rvel.first, config.rvel.second), std::max(config.rvel.first, config.rvel.second)) {
  const auto n = _count;

  _px.resize(n);
  _py.resize(n);
  _vx.resize(n);
  _vy.resize(n);
  _gx.resize(n);
  _gy.resize(n);
  _life.resize(n);
  _scale.resize(n);
  _angle.resize(n);
  _av.resize(n);
  _af.resize(n);
  _respawn.resize(n);

  _vertices.resize(n * 4);
  _indices.resize(n * 6);
}

float particle::x() const noexcept { return _x; }
void particle::set_x(float value) noexcept { _x = value; }
float particle::y() const noexcept { return _y; }
void particle::set_y(float value) noexcept { _y = value; }
bool particle::active() const noexcept { return _active; }

void particle::set_active(bool value) noexcept {
  _active = value;

  if (_sound) {
    value ? _sound->play() : _sound->fade(-1.f, .0f, 300);
  }
}

void particle::set_sound(class sound* sound, float distance, float volume) noexcept {
  _sound = sound;
  _distance = distance;
  _inverse_distance = 1.f / distance;
  _volume = volume;

  if (_active) {
    _sound->play();
  }
}

class sound* particle::sound() const noexcept {
  return _sound;
}

void particle::update(float delta) noexcept {
  const auto n = _count;
  auto& eng = rng();

  auto* noalias xs = _px.data();
  auto* noalias ys = _py.data();
  auto* noalias vxs = _vx.data();
  auto* noalias vys = _vy.data();
  auto* noalias gxs = _gx.data();
  auto* noalias gys = _gy.data();
  auto* noalias lifes = _life.data();
  auto* noalias scales = _scale.data();
  auto* noalias angles = _angle.data();
  auto* noalias avs = _av.data();
  auto* noalias afs = _af.data();

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

  if (_active) {
    const auto px = _x;
    const auto py = _y;
    auto* noalias respawn = _respawn.data();
    auto count = 0uz;

    for (auto i = 0uz; i < n; ++i) {
      respawn[count] = i;
      count += static_cast<size_t>(lifes[i] <= .0f);
    }

    for (auto j = 0uz; j < count; ++j) {
      const auto i = respawn[j];
      const auto r = _radiusd(eng);
      const auto a = _angled(eng);

      float sa, ca;
      sincos(a, sa, ca);

      xs[i] = px + _xspawnd(eng) + r * ca;
      ys[i] = py + _yspawnd(eng) + r * sa;
      vxs[i] = _xveld(eng);
      vys[i] = _yveld(eng);
      gxs[i] = _gxd(eng);
      gys[i] = _gyd(eng);
      avs[i] = _rotveld(eng);
      afs[i] = _rotforced(eng);
      lifes[i] = _lifed(eng);
      scales[i] = _scaled(eng);
      angles[i] = a;
    }
  }
}

void particle::draw() noexcept {
  const auto n = _count;
  const auto hw = _hw;
  const auto hh = _hh;
  const auto vw = viewport.width;
  const auto vh = viewport.height;
  const auto extent = std::max(hw, hh);
  auto* verts = _vertices.data();
  auto* idxs = _indices.data();

  const auto* noalias xs = _px.data();
  const auto* noalias ys = _py.data();
  const auto* noalias lifes = _life.data();
  const auto* noalias scales = _scale.data();
  const auto* noalias angles = _angle.data();

  auto visible = 0uz;

  for (auto i = 0uz; i < n; ++i) {
    const auto life = lifes[i];

    if (life <= .0f) [[unlikely]] {
      continue;
    }

    const auto sc = scales[i];
    const auto se = extent * sc;
    const auto px = xs[i] - viewport.x;
    const auto py = ys[i] - viewport.y;

    if (px + se < .0f || px - se > vw ||
        py + se < .0f || py - se > vh) [[unlikely]] {
      continue;
    }

    const auto alpha = std::min(life, 1.f);
    const auto shw = hw * sc;
    const auto shh = hh * sc;

    float sa, ca;
    sincos(angles[i], sa, ca);

    const SDL_FColor color = {1.f, 1.f, 1.f, alpha};

    const auto dx0 = -shw * ca + shh * sa;
    const auto dy0 = -shw * sa - shh * ca;
    const auto dx1 = shw * ca + shh * sa;
    const auto dy1 = shw * sa - shh * ca;

    const auto base = static_cast<int>(visible * 4);
    auto* vx = verts + visible * 4;
    auto* ix = idxs + visible * 6;

    vx[0] = {{px + dx0, py + dy0}, color, {.0f, .0f}};
    vx[1] = {{px + dx1, py + dy1}, color, {1.f, .0f}};
    vx[2] = {{px - dx0, py - dy0}, color, {1.f, 1.f}};
    vx[3] = {{px - dx1, py - dy1}, color, {.0f, 1.f}};

    ix[0] = base;
    ix[1] = base + 1;
    ix[2] = base + 2;
    ix[3] = base;
    ix[4] = base + 2;
    ix[5] = base + 3;

    ++visible;
  }

  if (visible > 0uz) [[likely]] {
    SDL_RenderGeometry(
      renderer,
      static_cast<SDL_Texture*>(*_texture),
      verts,
      static_cast<int>(visible * 4),
      idxs,
      static_cast<int>(visible * 6)
    );
  }

  if (_sound) [[likely]] {
    const auto cx = viewport.x + viewport.width  * .5f;
    const auto cy = viewport.y + viewport.height * .5f;
    const auto dx = _x - cx;
    const auto dy = _y - cy;
    const auto d = std::sqrt(dx * dx + dy * dy);
    const auto t = std::clamp(d * _inverse_distance, .0f, 1.f);
    const auto u = 1.f - t;
    _sound->set_volume(_volume * u * u);
    _sound->set_pan(std::clamp(dx * _inverse_distance, -1.f, 1.f) * .3f);
  }
}

void particle::wire() {
  metatable(L, "Particle", particle_index, particle_newindex);
}
