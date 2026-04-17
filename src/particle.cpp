#include "particle.hpp"

namespace {
namespace property {
  constexpr auto active = "active"_hs;
  constexpr auto x = "x"_hs;
  constexpr auto y = "y"_hs;
}

static int particle_index(lua_State* state) {
  const auto* self = *static_cast<particle**>(luaL_checkudata(state, 1, "Particle"));
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

static int particle_newindex(lua_State* state) {
  auto* self = *static_cast<particle**>(luaL_checkudata(state, 1, "Particle"));
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

particle::particle(const config& config, const pixmap& texture, float x, float y, bool active)
    : _x(x)
    , _y(y)
    , _half_width(static_cast<float>(texture.width()) * .5f)
    , _half_height(static_cast<float>(texture.height()) * .5f)
    , _active(active)
    , _count(config.count)
    , _texture(&texture)
    , _spawn_x_range(std::minmax(config.spawn_x.first, config.spawn_x.second))
    , _spawn_y_range(std::minmax(config.spawn_y.first, config.spawn_y.second))
    , _radius_range(std::minmax(config.radius.first, config.radius.second))
    , _angle_range(std::minmax(config.angle.first, config.angle.second))
    , _velocity_x_range(std::minmax(config.velocity_x.first, config.velocity_x.second))
    , _velocity_y_range(std::minmax(config.velocity_y.first, config.velocity_y.second))
    , _gravity_x_range(std::minmax(config.gravity_x.first, config.gravity_x.second))
    , _gravity_y_range(std::minmax(config.gravity_y.first, config.gravity_y.second))
    , _scale_range(std::minmax(config.scale.first, config.scale.second))
    , _life_range(std::minmax(config.life.first, config.life.second))
    , _rotation_force_range(std::minmax(config.rotation_force.first, config.rotation_force.second))
    , _rotation_velocity_range(std::minmax(config.rotation_velocity.first, config.rotation_velocity.second)) {
  assert(config.count > 0 && "particle count must be positive");

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

  _vertices.resize(n * 4);
  _indices.resize(n * 6);

  for (auto i = 0uz; i < n; ++i) {
    const auto base = static_cast<int>(i * 4);

    auto* ip = _indices.data() + i * 6;

    *ip++ = base; *ip++ = base + 1; *ip++ = base + 2;
    *ip++ = base; *ip++ = base + 2; *ip++ = base + 3;
  }
}

float particle::x() const noexcept { return _x; }
void particle::set_x(float value) noexcept { _x = value; }
float particle::y() const noexcept { return _y; }
void particle::set_y(float value) noexcept { _y = value; }
bool particle::active() const noexcept { return _active; }

void particle::set_active(bool value) noexcept {
  _active = value;
}

void particle::update(float delta) noexcept {
  const auto n = _count;
  const auto twopi = 2.f * std::numbers::pi_v<float>;

  auto* noalias xs = _position_x.data();
  auto* noalias ys = _position_y.data();
  auto* noalias vxs = _velocity_x.data();
  auto* noalias vys = _velocity_y.data();
  auto* noalias gxs = _gravity_x.data();
  auto* noalias gys = _gravity_y.data();
  auto* noalias lifes = _life.data();
  auto* noalias angles = _angle.data();
  auto* noalias avs = _angular_velocity.data();
  auto* noalias afs = _angular_force.data();

  for (auto i = 0uz; i < n; ++i) {
    lifes[i] -= delta;

    avs[i] += afs[i] * delta;

    auto ang = angles[i] + avs[i] * delta;
    if (ang >= twopi) ang -= twopi;
    if (ang < .0f) ang += twopi;
    angles[i] = ang;

    vxs[i] += gxs[i] * delta;
    vys[i] += gys[i] * delta;

    xs[i] += vxs[i] * delta;
    ys[i] += vys[i] * delta;
  }

  if (_active) {
    const auto px = _x;
    const auto py = _y;

    for (auto i = 0uz; i < n; ++i) {
      if (lifes[i] > .0f) [[likely]]
        continue;

      const auto radius = rng(_radius_range);
      const auto a = rng(_angle_range);

      float sa, ca;
      sincos(a, sa, ca);

      xs[i] = px + rng(_spawn_x_range) + radius * ca;
      ys[i] = py + rng(_spawn_y_range) + radius * sa;
      vxs[i] = rng(_velocity_x_range);
      vys[i] = rng(_velocity_y_range);
      gxs[i] = rng(_gravity_x_range);
      gys[i] = rng(_gravity_y_range);
      avs[i] = rng(_rotation_velocity_range);
      afs[i] = rng(_rotation_force_range);
      lifes[i] = rng(_life_range);
      _scale[i] = rng(_scale_range);
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
  auto* vertices = _vertices.data();
  auto* indices = _indices.data();

  const auto* noalias xs = _position_x.data();
  const auto* noalias ys = _position_y.data();
  const auto* noalias lifes = _life.data();
  const auto* noalias scales = _scale.data();
  const auto* noalias angles = _angle.data();

  auto* vp = vertices;

  for (auto i = 0uz; i < n; ++i) {
    if (lifes[i] <= .0f) [[unlikely]]
      continue;

    const auto sc = scales[i];
    const auto se = extent * sc;
    const auto px = xs[i] - viewport.x;
    const auto py = ys[i] - viewport.y;

    if (px - se > vw || px + se < .0f || py - se > vh || py + se < .0f) [[unlikely]]
      continue;

    const auto alpha = std::min(lifes[i], 1.f);
    const auto shw = hw * sc;
    const auto shh = hh * sc;

    float sa, ca;
    sincos(angles[i], sa, ca);

    const auto dx0 = -shw * ca + shh * sa;
    const auto dy0 = -shw * sa - shh * ca;
    const auto dx1 = shw * ca + shh * sa;
    const auto dy1 = shw * sa - shh * ca;

    const SDL_FColor color{1.f, 1.f, 1.f, alpha};

    *vp++ = SDL_Vertex{{px + dx0, py + dy0}, color, {.0f, .0f}};
    *vp++ = SDL_Vertex{{px + dx1, py + dy1}, color, {1.f, .0f}};
    *vp++ = SDL_Vertex{{px - dx0, py - dy0}, color, {1.f, 1.f}};
    *vp++ = SDL_Vertex{{px - dx1, py - dy1}, color, {.0f, 1.f}};
  }

  const auto nv = static_cast<int>(vp - vertices);

  SDL_RenderGeometry(
    renderer,
    static_cast<SDL_Texture*>(*_texture),
    vertices,
    nv,
    indices,
    nv / 4 * 6);
}

void particle::wire() {
  metatable(L, "Particle", particle_index, particle_newindex);
}
