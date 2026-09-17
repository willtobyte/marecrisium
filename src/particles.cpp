namespace {
constexpr auto crossing = .7035f;
constexpr auto curvature = -.814f;

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

static int index(lua_State* state) {
  const auto* self = *static_cast<particleemitter**>(luaL_checkudata(state, 1, "ParticleEmitter"));
  std::size_t length;
  const auto* data = luaL_checklstring(state, 2, &length);
  const std::string_view key{data, length};

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

static int newindex(lua_State* state) {
  auto* self = *static_cast<particleemitter**>(luaL_checkudata(state, 1, "ParticleEmitter"));
  std::size_t length;
  const auto* data = luaL_checklstring(state, 2, &length);
  const std::string_view key{data, length};

  if (key == "active")
    self->set_active(lua_toboolean(state, 3) != 0);
  else if (key == "x")
    self->set_x(static_cast<float>(luaL_checknumber(state, 3)));
  else if (key == "y")
    self->set_y(static_cast<float>(luaL_checknumber(state, 3)));

  return 0;
}

static std::pair<float, float> read_range(lua_State* state, const char* field) {
  float minimum = .0f, maximum = .0f;

  lua_getfield(state, -1, field);
  if (lua_istable(state, -1)) {
    lua_rawgeti(state, -1, 1);
    lua_rawgeti(state, -2, 2);

    minimum = static_cast<float>(lua_tonumber(state, -2));
    maximum = static_cast<float>(lua_tonumber(state, -1));

    lua_pop(state, 2);
  }

  lua_pop(state, 1);

  return {minimum, maximum};
}
}

particleemitter::particleemitter(const config& config, const pixmap& texture, float x, float y, bool active)
    : _count(config.count)
    , _texture(&texture)
    , _x(x)
    , _y(y)
    , _half{texture.width() * .5f, texture.height() * .5f}
    , _active(active)
    , _idle(!active)
    , _values(std::make_unique_for_overwrite<float[]>(config.count * std::to_underlying(slot::total)))
    , _vertices(std::make_unique_for_overwrite<SDL_Vertex[]>(config.count * 4))
    , _spawn_x_range(std::minmax(config.spawn.x.first, config.spawn.x.second))
    , _spawn_y_range(std::minmax(config.spawn.y.first, config.spawn.y.second))
    , _radius_range(std::minmax(config.radius.first, config.radius.second))
    , _angle_range(std::minmax(config.angle.first, config.angle.second))
    , _velocity_x_range(std::minmax(config.velocity.x.first, config.velocity.x.second))
    , _velocity_y_range(std::minmax(config.velocity.y.first, config.velocity.y.second))
    , _gravity_x_range(std::minmax(config.gravity.x.first, config.gravity.x.second))
    , _gravity_y_range(std::minmax(config.gravity.y.first, config.gravity.y.second))
    , _scale_range(std::minmax(config.scale.first, config.scale.second))
    , _life_range(std::minmax(config.life.first, config.life.second))
    , _rotation_force_range(std::minmax(config.rotation.force.first, config.rotation.force.second))
    , _rotation_velocity_range(std::minmax(config.rotation.velocity.first, config.rotation.velocity.second)) {
  assert(config.count % 4uz == 0 && "particleemitter count must be a multiple of four");
  [[assume(config.count % 4uz == 0)]];

  const auto count = _count;

  std::fill_n(_values.get(), count * std::to_underlying(slot::total), .0f);

  for (auto i = 0uz; i < count; ++i) {
    auto *vertices = _vertices.get() + i * 4;
    vertices[0] = SDL_Vertex{{}, {1.f, 1.f, 1.f, .0f}, {.0f, .0f}};
    vertices[1] = SDL_Vertex{{}, {1.f, 1.f, 1.f, .0f}, {1.f, .0f}};
    vertices[2] = SDL_Vertex{{}, {1.f, 1.f, 1.f, .0f}, {1.f, 1.f}};
    vertices[3] = SDL_Vertex{{}, {1.f, 1.f, 1.f, .0f}, {.0f, 1.f}};
  }
}

float particleemitter::x() const {
  return _x;
}

void particleemitter::set_x(float value) {
  _x = value;
}

float particleemitter::y() const {
  return _y;
}

void particleemitter::set_y(float value) {
  _y = value;
}

bool particleemitter::active() const {
  return _active;
}

void particleemitter::set_active(bool value) {
  _active = value;
  if (value)
    _idle = false;
}

void particleemitter::update(float delta) {
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

void particleemitter::draw(const int* indices) {
  if (_idle) [[unlikely]]
    return;

  const auto count = _count;

  const auto hw = _half.width;
  const auto hh = _half.height;
  auto* vertices = _vertices.get();

  const auto* values = _values.get();
  const auto* noalias xs = column<slot::x>(values, count);
  const auto* noalias ys = column<slot::y>(values, count);
  const auto* noalias life = column<slot::life>(values, count);
  const auto* noalias scales = column<slot::scale>(values, count);
  const auto* noalias angles = column<slot::angle>(values, count);

  auto* output = vertices;
  const auto zeroes = simde_mm_setzero_ps();
  const auto ones = simde_mm_set_ps1(1.f);
  const auto izeroes = simde_mm_setzero_si128();
  const auto iones = simde_mm_set_epi32(1, 1, 1, 1);
  const auto widths = simde_mm_set_ps1(hw);
  const auto heights = simde_mm_set_ps1(hh);

  for (auto i = 0uz; i < count; i += 4) {
    const auto lives = simde_mm_loadu_ps(life + i);
    const auto scale = simde_mm_loadu_ps(scales + i);
    const auto px = simde_mm_loadu_ps(xs + i);
    const auto py = simde_mm_loadu_ps(ys + i);
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

    for (auto lane = 0uz; lane < 4; ++lane, output += 4) {
      const auto a = alpha[lane];

      output[0].position = {x0[lane], y0[lane]};
      output[0].color.a = a;
      output[1].position = {x1[lane], y1[lane]};
      output[1].color.a = a;
      output[2].position = {x2[lane], y2[lane]};
      output[2].color.a = a;
      output[3].position = {x3[lane], y3[lane]};
      output[3].color.a = a;
    }
  }

  const auto nv = static_cast<int>(output - vertices);

  SDL_RenderGeometry(
    renderer,
    static_cast<SDL_Texture*>(*_texture),
    vertices,
    nv,
    indices,
    nv / 4 * 6);
}

void particleemitter::wire() {
  lua_createtable(L, 0, 3);
  lua_pushliteral(L, "ParticleEmitter");
  lua_setfield(L, -2, "__name");

  lua_pushcfunction(L, index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, newindex);
  lua_setfield(L, -2, "__newindex");
  lua_setfield(L, LUA_REGISTRYINDEX, "ParticleEmitter");
}

config* particlepool::get(std::string_view kind) {
  if (const auto it = _pool.find(kind); it != _pool.end()) [[likely]]
    return &it->second;

  struct config instance;
  const auto chunk = std::format("@particles/{}.lua", kind);
  const auto filename = std::string_view{chunk}.substr(1);
  const auto source = io::read(filename);
  if (luaL_loadbuffer(L, reinterpret_cast<const char*>(source.data()), source.size(), chunk.c_str()) != LUA_OK) [[unlikely]]
    throw std::runtime_error{lua_tostring(L, -1)};

  if (pcall(L, 0, 1) != LUA_OK) [[unlikely]]
    throw std::runtime_error{lua_tostring(L, -1)};

  lua_getfield(L, -1, "count");
  instance.count = static_cast<size_t>(lua_tonumber(L, -1));
  lua_pop(L, 1);

  lua_getfield(L, -1, "spawn");
  if (lua_istable(L, -1)) {
    instance.spawn.x = read_range(L, "x");
    instance.spawn.y = read_range(L, "y");
    instance.radius = read_range(L, "radius");
    instance.angle = read_range(L, "angle");
    instance.scale = read_range(L, "scale");
    instance.life = read_range(L, "life");
  }

  lua_pop(L, 1);

  lua_getfield(L, -1, "velocity");
  if (lua_istable(L, -1)) {
    instance.velocity.x = read_range(L, "x");
    instance.velocity.y = read_range(L, "y");
  }

  lua_pop(L, 1);

  lua_getfield(L, -1, "gravity");
  if (lua_istable(L, -1)) {
    instance.gravity.x = read_range(L, "x");
    instance.gravity.y = read_range(L, "y");
  }

  lua_pop(L, 1);

  lua_getfield(L, -1, "rotation");
  if (lua_istable(L, -1)) {
    instance.rotation.force = read_range(L, "force");
    instance.rotation.velocity = read_range(L, "velocity");
  }

  lua_pop(L, 1);
  lua_pop(L, 1);

  auto* result = &_pool.emplace(kind, std::move(instance)).first->second;

  return result;
}

void particlepool::clear() {
  _pool.clear();
}

particleemitter* particlesystem::add(std::string_view name, std::string_view kind, float x, float y, bool active) {
  const auto it = _particles.find(name);
  if (it != _particles.end())
    return &it->second;

  const auto* cfg = depot->get<config>(kind);
  const auto* texture = depot->get<pixmap>(std::format("particles/{}", kind));
  _indices.reserve(std::max(_indices.size(), cfg->count * 6));
  for (auto i = _indices.size() / 6; i < cfg->count; ++i) {
    const auto base = static_cast<int>(i * 4);
    _indices.insert(_indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
  }

  auto* result = &_particles.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(name),
    std::forward_as_tuple(*cfg, *texture, x, y, active)
  ).first->second;

  _order.emplace_back(result);

  return result;
}

void particlesystem::update(float delta) {
  for (auto* emitter : _order)
    emitter->update(delta);
}

void particlesystem::draw() {
  for (auto* emitter : _order)
    emitter->draw(_indices.data());
}

void particlesystem::clear() {
  _order.clear();
  _particles.clear();
}
