#include "particle.hpp"

namespace {

constexpr float TWO_PI = 6.28318530718f;

namespace property {
  constexpr auto active = "active"_hs;
  constexpr auto x = "x"_hs;
  constexpr auto y = "y"_hs;
}

int particle_index(lua_State* state) {
  const auto* self = *static_cast<particle**>(lua_touserdata(state, 1));
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

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

    default:
      return lua_pushnil(state), 1;
  }
}

int particle_newindex(lua_State* state) {
  auto* self = *static_cast<particle**>(lua_touserdata(state, 1));
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

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
    , _half_width(static_cast<float>(texture.width()) * .5f)
    , _half_height(static_cast<float>(texture.height()) * .5f)
    , _active(active)
    , _count(config.count)
    , _texture(&texture)
    , _spawn_x_distribution(std::min(config.spawn_x.first, config.spawn_x.second), std::max(config.spawn_x.first, config.spawn_x.second))
    , _spawn_y_distribution(std::min(config.spawn_y.first, config.spawn_y.second), std::max(config.spawn_y.first, config.spawn_y.second))
    , _radius_distribution(std::min(config.radius.first, config.radius.second), std::max(config.radius.first, config.radius.second))
    , _angle_distribution(std::min(config.angle.first, config.angle.second), std::max(config.angle.first, config.angle.second))
    , _velocity_x_distribution(std::min(config.velocity_x.first, config.velocity_x.second), std::max(config.velocity_x.first, config.velocity_x.second))
    , _velocity_y_distribution(std::min(config.velocity_y.first, config.velocity_y.second), std::max(config.velocity_y.first, config.velocity_y.second))
    , _gravity_x_distribution(std::min(config.gravity_x.first, config.gravity_x.second), std::max(config.gravity_x.first, config.gravity_x.second))
    , _gravity_y_distribution(std::min(config.gravity_y.first, config.gravity_y.second), std::max(config.gravity_y.first, config.gravity_y.second))
    , _scale_distribution(std::min(config.scale.first, config.scale.second), std::max(config.scale.first, config.scale.second))
    , _life_distribution(std::min(config.life.first, config.life.second), std::max(config.life.first, config.life.second))
    , _rotation_force_distribution(std::min(config.rotation_force.first, config.rotation_force.second), std::max(config.rotation_force.first, config.rotation_force.second))
    , _rotation_velocity_distribution(std::min(config.rotation_velocity.first, config.rotation_velocity.second), std::max(config.rotation_velocity.first, config.rotation_velocity.second)) {
  const auto n = _count;

  _position_x.resize(n);
  _position_y.resize(n);
  _velocity_x.resize(n);
  _velocity_y.resize(n);
  _gravity_x.resize(n);
  _gravity_y.resize(n);
  _life.resize(n);
  _scale.resize(n);
  _angle.resize(n);
  _angular_velocity.resize(n);
  _angular_force.resize(n);
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

  auto* noalias xs = _position_x.data();
  auto* noalias ys = _position_y.data();
  auto* noalias vxs = _velocity_x.data();
  auto* noalias vys = _velocity_y.data();
  auto* noalias gxs = _gravity_x.data();
  auto* noalias gys = _gravity_y.data();
  auto* noalias lifes = _life.data();
  auto* noalias scales = _scale.data();
  auto* noalias angles = _angle.data();
  auto* noalias avs = _angular_velocity.data();
  auto* noalias afs = _angular_force.data();

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
      const auto r = _radius_distribution(eng);
      const auto a = _angle_distribution(eng);

      float sa, ca;
      sincos(a, sa, ca);

      xs[i] = px + _spawn_x_distribution(eng) + r * ca;
      ys[i] = py + _spawn_y_distribution(eng) + r * sa;
      vxs[i] = _velocity_x_distribution(eng);
      vys[i] = _velocity_y_distribution(eng);
      gxs[i] = _gravity_x_distribution(eng);
      gys[i] = _gravity_y_distribution(eng);
      avs[i] = _rotation_velocity_distribution(eng);
      afs[i] = _rotation_force_distribution(eng);
      lifes[i] = _life_distribution(eng);
      scales[i] = _scale_distribution(eng);
      angles[i] = a;
    }
  }
}

void particle::draw() noexcept {
  const auto n = _count;
  const auto hw = _half_width;
  const auto hh = _half_height;
  const auto vw = viewport.width;
  const auto vh = viewport.height;
  const auto extent = std::max(hw, hh);
  auto* verts = _vertices.data();
  auto* indices = _indices.data();

  const auto* noalias xs = _position_x.data();
  const auto* noalias ys = _position_y.data();
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
    auto* ix = indices + visible * 6;

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

  SDL_RenderGeometry(
    renderer,
    static_cast<SDL_Texture*>(*_texture),
    verts,
    static_cast<int>(visible * 4),
    indices,
    static_cast<int>(visible * 6)
  );

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
