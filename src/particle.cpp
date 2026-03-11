#include "particle.hpp"

namespace {

constexpr float TWO_PI = 6.28318530718f;

int particle_index(lua_State* state) {
  const auto* self = *static_cast<particle**>(luaL_checkudata(state, 1, "Particle"));
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "active") {
    lua_pushboolean(state, self->active());
    return 1;
  }

  if (key == "x") {
    lua_pushnumber(state, static_cast<lua_Number>(self->x()));
    return 1;
  }

  if (key == "y") {
    lua_pushnumber(state, static_cast<lua_Number>(self->y()));
    return 1;
  }

  return lua_pushnil(state), 1;
}

int particle_newindex(lua_State* state) {
  auto* self = *static_cast<particle**>(luaL_checkudata(state, 1, "Particle"));
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "active") {
    self->set_active(lua_toboolean(state, 3) != 0);
    return 0;
  }

  if (key == "x") {
    self->set_x(static_cast<float>(luaL_checknumber(state, 3)));
    return 0;
  }

  if (key == "y") {
    self->set_y(static_cast<float>(luaL_checknumber(state, 3)));
    return 0;
  }

  return 0;
}

}

std::mt19937& particle::rng() noexcept {
  static std::mt19937 engine{std::random_device{}()};
  return engine;
}

particle::particle(const config& cfg, const pixmap& texture, float x, float y, bool active)
    : _texture(&texture)
    , _x(x)
    , _y(y)
    , _hw(static_cast<float>(texture.width()) * .5f)
    , _hh(static_cast<float>(texture.height()) * .5f)
    , _active(active)
    , _count(cfg.count)
    , _xspawnd(std::min(cfg.xspawn.first, cfg.xspawn.second), std::max(cfg.xspawn.first, cfg.xspawn.second))
    , _yspawnd(std::min(cfg.yspawn.first, cfg.yspawn.second), std::max(cfg.yspawn.first, cfg.yspawn.second))
    , _radiusd(std::min(cfg.radius.first, cfg.radius.second), std::max(cfg.radius.first, cfg.radius.second))
    , _angled(std::min(cfg.angle.first, cfg.angle.second), std::max(cfg.angle.first, cfg.angle.second))
    , _xveld(std::min(cfg.xvel.first, cfg.xvel.second), std::max(cfg.xvel.first, cfg.xvel.second))
    , _yveld(std::min(cfg.yvel.first, cfg.yvel.second), std::max(cfg.yvel.first, cfg.yvel.second))
    , _gxd(std::min(cfg.gx.first, cfg.gx.second), std::max(cfg.gx.first, cfg.gx.second))
    , _gyd(std::min(cfg.gy.first, cfg.gy.second), std::max(cfg.gy.first, cfg.gy.second))
    , _scaled(std::min(cfg.scale.first, cfg.scale.second), std::max(cfg.scale.first, cfg.scale.second))
    , _lifed(std::min(cfg.life.first, cfg.life.second), std::max(cfg.life.first, cfg.life.second))
    , _rotforced(std::min(cfg.rforce.first, cfg.rforce.second), std::max(cfg.rforce.first, cfg.rforce.second))
    , _rotveld(std::min(cfg.rvel.first, cfg.rvel.second), std::max(cfg.rvel.first, cfg.rvel.second)) {
  if (luaL_newmetatable(L, "Particle")) {
    lua_pushcfunction(L, particle_index);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, particle_newindex);
    lua_setfield(L, -2, "__newindex");
  }
  lua_pop(L, 1);

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
void particle::set_active(bool value) noexcept { _active = value; }

void particle::update(float delta) noexcept {
  const auto n = _count;
  auto& eng = rng();

  auto* RESTRICT xs = _px.data();
  auto* RESTRICT ys = _py.data();
  auto* RESTRICT vxs = _vx.data();
  auto* RESTRICT vys = _vy.data();
  auto* RESTRICT gxs = _gx.data();
  auto* RESTRICT gys = _gy.data();
  auto* RESTRICT lifes = _life.data();
  auto* RESTRICT scales = _scale.data();
  auto* RESTRICT angles = _angle.data();
  auto* RESTRICT avs = _av.data();
  auto* RESTRICT afs = _af.data();

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
    auto* RESTRICT respawn = _respawn.data();
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

void particle::draw(float camera_x, float camera_y) const noexcept {
  const auto n = _count;
  const auto hw = _hw;
  const auto hh = _hh;
  const auto vw = viewport.width;
  const auto vh = viewport.height;
  const auto extent = std::max(hw, hh);
  auto* verts = _vertices.data();
  auto* idxs = _indices.data();

  const auto* RESTRICT xs = _px.data();
  const auto* RESTRICT ys = _py.data();
  const auto* RESTRICT lifes = _life.data();
  const auto* RESTRICT scales = _scale.data();
  const auto* RESTRICT angles = _angle.data();

  auto visible = 0uz;

  for (auto i = 0uz; i < n; ++i) {
    const auto life = lifes[i];

    if (life <= .0f) [[unlikely]] {
      continue;
    }

    const auto sc = scales[i];
    const auto se = extent * sc;
    const auto px = xs[i] - camera_x;
    const auto py = ys[i] - camera_y;

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
}
