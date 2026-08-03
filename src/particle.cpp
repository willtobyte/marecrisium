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
  return range.first == range.second ? range.first : prng(range);
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
  [[assume(config.count > 0)]];
  assert(config.count % 4uz == 0 && "particle count must be a multiple of four");
  [[assume(config.count % 4uz == 0)]];

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
  assert(count > 0 && "particle count must be positive");
  [[assume(count > 0)]];
  assert(count % 4uz == 0 && "particle count must be a multiple of four");
  [[assume(count % 4uz == 0)]];
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
  assert(count > 0 && "particle count must be positive");
  [[assume(count > 0)]];
  assert(count % 4uz == 0 && "particle count must be a multiple of four");
  [[assume(count % 4uz == 0)]];
  const auto hw = _half_width;
  const auto hh = _half_height;
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

  auto* out = vertices;
  const auto vx = simde_mm_set_ps1(viewport.x);
  const auto vy = simde_mm_set_ps1(viewport.y);
  const auto zeroes = simde_mm_setzero_ps();
  const auto ones = simde_mm_set_ps1(1.f);
  const auto izeroes = simde_mm_setzero_si128();
  const auto iones = simde_mm_set_epi32(1, 1, 1, 1);
  const auto widths = simde_mm_set_ps1(hw);
  const auto heights = simde_mm_set_ps1(hh);

  for (auto i = 0uz; i < count; i += 4) {
    const auto lives = simde_mm_loadu_ps(life + i);
    const auto scale = simde_mm_loadu_ps(scales + i);
    const auto px = simde_mm_sub_ps(simde_mm_loadu_ps(xs + i), vx);
    const auto py = simde_mm_sub_ps(simde_mm_loadu_ps(ys + i), vy);
    const auto alphas = simde_mm_max_ps(simde_mm_min_ps(lives, ones), zeroes);
    const auto raw = simde_mm_mul_ps(
      simde_mm_loadu_ps(angles + i), simde_mm_set_ps1(2.f * std::numbers::inv_pi_v<float>));
    auto quadrants = simde_mm_cvttps_epi32(raw);
    const auto negative = simde_mm_castps_si128(simde_mm_cmplt_ps(raw, zeroes));
    quadrants = simde_mm_sub_epi32(quadrants, simde_mm_and_si128(negative, iones));
    const auto reduced = simde_mm_sub_ps(
      simde_mm_sub_ps(raw, simde_mm_cvtepi32_ps(quadrants)), simde_mm_set_ps1(.5f));
    const auto shared = simde_mm_add_ps(
      simde_mm_mul_ps(simde_mm_mul_ps(reduced, reduced), simde_mm_set_ps1(curvature)),
      simde_mm_set_ps1(crossing));
    auto sines = simde_mm_castps_si128(simde_mm_add_ps(shared, reduced));
    auto cosines = simde_mm_castps_si128(simde_mm_sub_ps(shared, reduced));
    const auto index = simde_mm_and_si128(quadrants, simde_mm_set_epi32(3, 3, 3, 3));
    const auto selection = simde_mm_sub_epi32(izeroes, simde_mm_and_si128(index, iones));
    const auto difference = simde_mm_and_si128(simde_mm_xor_si128(sines, cosines), selection);
    sines = simde_mm_xor_si128(sines, difference);
    cosines = simde_mm_xor_si128(cosines, difference);
    const auto ssign = simde_mm_slli_epi32(
      simde_mm_and_si128(index, simde_mm_set_epi32(2, 2, 2, 2)), 30);
    const auto csign = simde_mm_slli_epi32(
      simde_mm_and_si128(simde_mm_add_epi32(index, iones), simde_mm_set_epi32(2, 2, 2, 2)), 30);
    const auto sine = simde_mm_castsi128_ps(simde_mm_xor_si128(sines, ssign));
    const auto cosine = simde_mm_castsi128_ps(simde_mm_xor_si128(cosines, csign));
    const auto sw = simde_mm_mul_ps(widths, scale);
    const auto sh = simde_mm_mul_ps(heights, scale);
    const auto swc = simde_mm_mul_ps(sw, cosine);
    const auto shs = simde_mm_mul_ps(sh, sine);
    const auto sws = simde_mm_mul_ps(sw, sine);
    const auto shc = simde_mm_mul_ps(sh, cosine);
    const auto dx0 = simde_mm_sub_ps(shs, swc);
    const auto dy0 = simde_mm_sub_ps(zeroes, simde_mm_add_ps(sws, shc));
    const auto dx1 = simde_mm_add_ps(swc, shs);
    const auto dy1 = simde_mm_sub_ps(sws, shc);

    alignas(16) float x0[4], y0[4], x1[4], y1[4], x2[4], y2[4], x3[4], y3[4], alpha[4];
    simde_mm_storeu_ps(x0, simde_mm_add_ps(px, dx0));
    simde_mm_storeu_ps(y0, simde_mm_add_ps(py, dy0));
    simde_mm_storeu_ps(x1, simde_mm_add_ps(px, dx1));
    simde_mm_storeu_ps(y1, simde_mm_add_ps(py, dy1));
    simde_mm_storeu_ps(x2, simde_mm_sub_ps(px, dx0));
    simde_mm_storeu_ps(y2, simde_mm_sub_ps(py, dy0));
    simde_mm_storeu_ps(x3, simde_mm_sub_ps(px, dx1));
    simde_mm_storeu_ps(y3, simde_mm_sub_ps(py, dy1));
    simde_mm_storeu_ps(alpha, alphas);

    for (auto lane = 0uz; lane < 4; ++lane) {
      auto* output = out + lane * 4;
      output[0].position = {x0[lane], y0[lane]};
      output[0].color.a = alpha[lane];
      output[1].position = {x1[lane], y1[lane]};
      output[1].color.a = alpha[lane];
      output[2].position = {x2[lane], y2[lane]};
      output[2].color.a = alpha[lane];
      output[3].position = {x3[lane], y3[lane]};
      output[3].color.a = alpha[lane];
    }

    out += 16;
  }

  const auto nv = static_cast<int>(out - vertices);
  assert(nv > 0 && "particle vertex count must be positive");
  [[assume(nv > 0)]];

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
