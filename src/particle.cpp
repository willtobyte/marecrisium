namespace {
constexpr auto crossing = .7035f;
constexpr auto curvature = -.814f;

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

[[nodiscard]] static float sample(std::pair<float, float> range) {
  return range.first == range.second ? range.first : rng(range);
}

static void sincos(float angle, float& sine, float& cosine) noexcept {
  const auto raw = angle * (2.f * std::numbers::inv_pi_v<float>);
  const auto quadrant = static_cast<int>(raw) - static_cast<int>(raw < .0f);
  const auto reduced = raw - static_cast<float>(quadrant) - .5f;
  const auto shared = curvature * reduced * reduced + crossing;
  auto sin = std::bit_cast<std::uint32_t>(shared + reduced);
  auto cos = std::bit_cast<std::uint32_t>(shared - reduced);
  const auto index = quadrant & 3;
  const auto selection = 0u - static_cast<std::uint32_t>(index & 1);
  const auto difference = (sin ^ cos) & selection;
  sin ^= difference;
  cos ^= difference;
  const auto ss = static_cast<std::uint32_t>(index & 2) << 30;
  const auto cs = static_cast<std::uint32_t>((index + 1) & 2) << 30;
  sine = std::bit_cast<float>(sin ^ ss);
  cosine = std::bit_cast<float>(cos ^ cs);
}

static int index(lua_State* state) {
  const auto* self = *static_cast<particle**>(luaL_checkudata(state, 1, "Particle"));
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  switch (id) {
    case lookup::active:
      lua_pushboolean(state, self->active());
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

static int newindex(lua_State* state) {
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

  const auto count = _count;

  std::fill_n(_values.get(), count * std::to_underlying(slot::total), .0f);

  for (auto i = 0uz; i < count; ++i) {
    const auto base = static_cast<int>(i * 4);

    auto *vertices = _vertices.get() + i * 4;
    vertices[0] = SDL_Vertex{{}, {1.f, 1.f, 1.f, .0f}, {.0f, .0f}};
    vertices[1] = SDL_Vertex{{}, {1.f, 1.f, 1.f, .0f}, {1.f, .0f}};
    vertices[2] = SDL_Vertex{{}, {1.f, 1.f, 1.f, .0f}, {1.f, 1.f}};
    vertices[3] = SDL_Vertex{{}, {1.f, 1.f, 1.f, .0f}, {.0f, 1.f}};

    auto *indices = _indices.get() + i * 6;
    indices[0] = base;
    indices[1] = base + 1;
    indices[2] = base + 2;
    indices[3] = base;
    indices[4] = base + 2;
    indices[5] = base + 3;
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

  const auto count = _count;
  const auto twopi = 2.f * std::numbers::pi_v<float>;

  auto* values = _values.get();
  auto* noalias xs = column<slot::x>(values, count);
  auto* noalias ys = column<slot::y>(values, count);
  auto* noalias vxs = column<slot::vx>(values, count);
  auto* noalias vys = column<slot::vy>(values, count);
  auto* noalias gxs = column<slot::gx>(values, count);
  auto* noalias gys = column<slot::gy>(values, count);
  auto* noalias life = column<slot::life>(values, count);
  auto* noalias scales = column<slot::scale>(values, count);
  auto* noalias angles = column<slot::angle>(values, count);
  auto* noalias avs = column<slot::av>(values, count);
  auto* noalias afs = column<slot::af>(values, count);

  assert(xs != nullptr && "particle X storage must exist");
  [[assume(xs != nullptr)]];
  assert(ys != nullptr && "particle Y storage must exist");
  [[assume(ys != nullptr)]];
  assert(vxs != nullptr && "particle X velocity storage must exist");
  [[assume(vxs != nullptr)]];
  assert(vys != nullptr && "particle Y velocity storage must exist");
  [[assume(vys != nullptr)]];
  assert(life != nullptr && "particle life storage must exist");
  [[assume(life != nullptr)]];
  assert(angles != nullptr && "particle angle storage must exist");
  [[assume(angles != nullptr)]];

  auto idle = !_active;
  for (auto i = 0uz; i < count; ++i) {
    life[i] -= delta;
    idle &= life[i] <= .0f;

    avs[i] += afs[i] * delta;

    auto angle = angles[i] + avs[i] * delta;
    if (angle >= twopi) angle -= twopi;
    if (angle < .0f) angle += twopi;
    angles[i] = angle;

    vxs[i] += gxs[i] * delta;
    vys[i] += gys[i] * delta;

    xs[i] += vxs[i] * delta;
    ys[i] += vys[i] * delta;
  }

  if (_active) {
    const auto px = _x;
    const auto py = _y;

    for (auto i = 0uz; i < count; ++i) {
      if (life[i] > .0f) [[likely]]
        continue;

      const auto radius = sample(_radius_range);
      const auto angle = sample(_angle_range);

      float sine, cosine;
      sincos(angle, sine, cosine);

      xs[i] = px + sample(_spawn_x_range) + radius * cosine;
      ys[i] = py + sample(_spawn_y_range) + radius * sine;
      vxs[i] = sample(_velocity_x_range);
      vys[i] = sample(_velocity_y_range);
      gxs[i] = sample(_gravity_x_range);
      gys[i] = sample(_gravity_y_range);
      avs[i] = sample(_rotation_velocity_range);
      afs[i] = sample(_rotation_force_range);
      life[i] = sample(_life_range);
      scales[i] = sample(_scale_range);
      angles[i] = angle;
    }
  }

  _idle = idle;
}

void particle::draw() {
  if (_idle) [[unlikely]]
    return;

  const auto count = _count;
  const auto hw = _half_width;
  const auto hh = _half_height;
  const auto vw = viewport.width;
  const auto vh = viewport.height;
  const auto extent = std::max(hw, hh);
  auto* vertices = _vertices.get();
  auto* indices = _indices.get();

  const auto* values = _values.get();
  const auto* noalias xs = column<slot::x>(values, count);
  const auto* noalias ys = column<slot::y>(values, count);
  const auto* noalias life = column<slot::life>(values, count);
  const auto* noalias scales = column<slot::scale>(values, count);
  const auto* noalias angles = column<slot::angle>(values, count);

  assert(xs != nullptr && "particle X storage must exist");
  [[assume(xs != nullptr)]];
  assert(ys != nullptr && "particle Y storage must exist");
  [[assume(ys != nullptr)]];
  assert(life != nullptr && "particle life storage must exist");
  [[assume(life != nullptr)]];
  assert(scales != nullptr && "particle scale storage must exist");
  [[assume(scales != nullptr)]];
  assert(angles != nullptr && "particle angle storage must exist");
  [[assume(angles != nullptr)]];
  assert(vertices != nullptr && "particle vertex storage must exist");
  [[assume(vertices != nullptr)]];

  auto *out = vertices;

  for (auto i = 0uz; i < count; ++i) {
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

    float sine, cosine;
    sincos(angles[i], sine, cosine);

    const auto dx0 = -sw * cosine + sh * sine;
    const auto dy0 = -sw * sine - sh * cosine;
    const auto dx1 = sw * cosine + sh * sine;
    const auto dy1 = sw * sine - sh * cosine;

    out[0].position = {px + dx0, py + dy0};
    out[0].color.a = alpha;
    out[1].position = {px + dx1, py + dy1};
    out[1].color.a = alpha;
    out[2].position = {px - dx0, py - dy0};
    out[2].color.a = alpha;
    out[3].position = {px - dx1, py - dy1};
    out[3].color.a = alpha;
    out += 4;
  }

  const auto nv = static_cast<int>(out - vertices);
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
  luaL_newmetatable(L, "Particle");
  lua_pushliteral(L, "Particle");
  lua_setfield(L, -2, "__name");

  lua_pushcfunction(L, index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, newindex);
  lua_setfield(L, -2, "__newindex");
  lua_pop(L, 1);
}
