namespace {
namespace lookup {
  constexpr auto active = "active"_hs;
  constexpr auto x = "x"_hs;
  constexpr auto y = "y"_hs;
}

enum class slot : size_t {
  x,
  y,
  vx,
  vy,
  gx,
  gy,
  life,
  scale,
  angle,
  av,
  af,
  total,
};

template<slot S, typename T>
static T* column(T* values, size_t count) noexcept {
  return values + count * std::to_underlying(S);
}

static int particle_index(lua_State* state) {
  const auto* self = *static_cast<particle**>(luaL_checkudata(state, 1, "Particle"));
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  switch (id) {
    case lookup::active:
      lua_pushboolean(state, self->active() ? 1 : 0);
      return 1;

    case lookup::x:
      lua_pushnumber(state, static_cast<lua_Number>(self->x()));
      return 1;

    case lookup::y:
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
    case lookup::active:
      self->set_active(lua_toboolean(state, 3) != 0);
      return 0;

    case lookup::x:
      self->set_x(static_cast<float>(luaL_checknumber(state, 3)));
      return 0;

    case lookup::y:
      self->set_y(static_cast<float>(luaL_checknumber(state, 3)));
      return 0;

    default:
      return 0;
  }
}
}

particle::particle(const config& config, const pixmap& texture, float x, float y, bool active)
    : _count(config.count)
    , _texture(&texture)
    , _x(x)
    , _y(y)
    , _half_width(static_cast<float>(texture.width()) * .5f)
    , _half_height(static_cast<float>(texture.height()) * .5f)
    , _active(active)
    , _idle(!active)
    , _values(std::make_unique_for_overwrite<float[]>(config.count * std::to_underlying(slot::total)))
    , _vertices(std::make_unique_for_overwrite<SDL_Vertex[]>(config.count * 4))
    , _indices(std::make_unique_for_overwrite<int[]>(config.count * 6))
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

  std::fill_n(_values.get(), n * std::to_underlying(slot::total), .0f);

  for (auto i = 0uz; i < n; ++i) {
    const auto base = static_cast<int>(i * 4);

    auto* ip = _indices.get() + i * 6;

    *ip++ = base; *ip++ = base + 1; *ip++ = base + 2;
    *ip++ = base; *ip++ = base + 2; *ip++ = base + 3;
  }
}

float particle::x() const { return _x; }
void particle::set_x(float value) { _x = value; }
float particle::y() const { return _y; }
void particle::set_y(float value) { _y = value; }
bool particle::active() const { return _active; }

void particle::set_active(bool value) {
  _active = value;
  if (value)
    _idle = false;
}

void particle::update(float delta) {
  if (_idle) [[unlikely]]
    return;

  const auto n = _count;
  const auto twopi = 2.f * std::numbers::pi_v<float>;

  auto* values = _values.get();
  auto* noalias xs = column<slot::x>(values, n);
  auto* noalias ys = column<slot::y>(values, n);
  auto* noalias vxs = column<slot::vx>(values, n);
  auto* noalias vys = column<slot::vy>(values, n);
  auto* noalias gxs = column<slot::gx>(values, n);
  auto* noalias gys = column<slot::gy>(values, n);
  auto* noalias life = column<slot::life>(values, n);
  auto* noalias scales = column<slot::scale>(values, n);
  auto* noalias angles = column<slot::angle>(values, n);
  auto* noalias avs = column<slot::av>(values, n);
  auto* noalias afs = column<slot::af>(values, n);

  [[assume(xs != nullptr)]];
  [[assume(ys != nullptr)]];
  [[assume(vxs != nullptr)]];
  [[assume(vys != nullptr)]];
  [[assume(life != nullptr)]];
  [[assume(angles != nullptr)]];

  auto idle = !_active;
  for (auto i = 0uz; i < n; ++i) {
    life[i] -= delta;
    idle = idle && life[i] <= .0f;

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
      if (life[i] > .0f) [[likely]]
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
      life[i] = rng(_life_range);
      scales[i] = rng(_scale_range);
      angles[i] = a;
    }
  }

  _idle = idle;
}

void particle::draw() {
  if (_idle) [[unlikely]]
    return;

  const auto n = _count;
  const auto hw = _half_width;
  const auto hh = _half_height;
  const auto vw = viewport.width;
  const auto vh = viewport.height;
  const auto extent = std::max(hw, hh);
  auto* vertices = _vertices.get();
  auto* indices = _indices.get();

  const auto* values = _values.get();
  const auto* noalias xs = column<slot::x>(values, n);
  const auto* noalias ys = column<slot::y>(values, n);
  const auto* noalias life = column<slot::life>(values, n);
  const auto* noalias scales = column<slot::scale>(values, n);
  const auto* noalias angles = column<slot::angle>(values, n);

  [[assume(xs != nullptr)]];
  [[assume(ys != nullptr)]];
  [[assume(life != nullptr)]];
  [[assume(scales != nullptr)]];
  [[assume(angles != nullptr)]];
  [[assume(vertices != nullptr)]];

  auto* vp = vertices;

  for (auto i = 0uz; i < n; ++i) {
    if (life[i] <= .0f) [[unlikely]]
      continue;

    const auto sc = scales[i];
    const auto se = extent * sc;
    const auto px = xs[i] - viewport.x;
    const auto py = ys[i] - viewport.y;

    if (px - se > vw || px + se < .0f || py - se > vh || py + se < .0f) [[unlikely]]
      continue;

    const auto alpha = std::min(life[i], 1.f);
    const auto sw = hw * sc;
    const auto sh = hh * sc;

    float sa, ca;
    sincos(angles[i], sa, ca);

    const auto dx0 = -sw * ca + sh * sa;
    const auto dy0 = -sw * sa - sh * ca;
    const auto dx1 = sw * ca + sh * sa;
    const auto dy1 = sw * sa - sh * ca;

    const SDL_FColor color{1.f, 1.f, 1.f, alpha};

    *vp++ = SDL_Vertex{{px + dx0, py + dy0}, color, {.0f, .0f}};
    *vp++ = SDL_Vertex{{px + dx1, py + dy1}, color, {1.f, .0f}};
    *vp++ = SDL_Vertex{{px - dx0, py - dy0}, color, {1.f, 1.f}};
    *vp++ = SDL_Vertex{{px - dx1, py - dy1}, color, {.0f, 1.f}};
  }

  const auto nv = static_cast<int>(vp - vertices);
  if (nv == 0) [[unlikely]]
    return;

  SDL_RenderGeometry(
    renderer,
    static_cast<SDL_Texture*>(*_texture),
    vertices,
    nv,
    indices,
    nv / 4 * 6);
}

void particle::wire() {
  binding::metatable(L, "Particle", particle_index, particle_newindex);
}
