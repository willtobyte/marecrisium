#include "particle.hpp"

namespace {
namespace property {
  constexpr auto active = "active"_hs;
  constexpr auto x = "x"_hs;
  constexpr auto y = "y"_hs;
}

int particle_index(lua_State* state) {
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

int particle_newindex(lua_State* state) {
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
  assert(config.count % 4 == 0 && "particle count must be a multiple of 4 for SIMD");

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
  _random.resize(n * 12);
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
}



void particle::update(float delta) noexcept {
  const auto n = _count;

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

  const auto vdelta = simde_mm_set1_ps(delta);
  const auto vtwopi = simde_mm_set1_ps(2.f * std::numbers::pi_v<float>);
  const auto vzero  = simde_mm_setzero_ps();

  for (auto i = 0uz; i < n; i += 4) {
    auto vlife = simde_mm_loadu_ps(lifes + i);
    vlife = simde_mm_sub_ps(vlife, vdelta);
    simde_mm_storeu_ps(lifes + i, vlife);

    auto vav = simde_mm_loadu_ps(avs + i);
    vav = simde_mm_add_ps(vav, simde_mm_mul_ps(simde_mm_loadu_ps(afs + i), vdelta));
    simde_mm_storeu_ps(avs + i, vav);

    auto vang = simde_mm_loadu_ps(angles + i);
    vang = simde_mm_add_ps(vang, simde_mm_mul_ps(vav, vdelta));
    vang = simde_mm_sub_ps(vang, simde_mm_and_ps(vtwopi, simde_mm_cmpge_ps(vang, vtwopi)));
    vang = simde_mm_add_ps(vang, simde_mm_and_ps(vtwopi, simde_mm_cmplt_ps(vang, vzero)));
    simde_mm_storeu_ps(angles + i, vang);

    auto vvx = simde_mm_loadu_ps(vxs + i);
    vvx = simde_mm_add_ps(vvx, simde_mm_mul_ps(simde_mm_loadu_ps(gxs + i), vdelta));
    simde_mm_storeu_ps(vxs + i, vvx);

    auto vvy = simde_mm_loadu_ps(vys + i);
    vvy = simde_mm_add_ps(vvy, simde_mm_mul_ps(simde_mm_loadu_ps(gys + i), vdelta));
    simde_mm_storeu_ps(vys + i, vvy);

    simde_mm_storeu_ps(xs + i, simde_mm_add_ps(simde_mm_loadu_ps(xs + i), simde_mm_mul_ps(vvx, vdelta)));
    simde_mm_storeu_ps(ys + i, simde_mm_add_ps(simde_mm_loadu_ps(ys + i), simde_mm_mul_ps(vvy, vdelta)));
  }

  if (_active) {
    const auto px = _x;
    const auto py = _y;
    auto* noalias respawn = _respawn.data();
    auto count = 0uz;

    for (auto i = 0uz; i < n; i += 4) {
      const auto vlife = simde_mm_loadu_ps(lifes + i);
      const auto dead = simde_mm_movemask_ps(simde_mm_cmple_ps(vlife, vzero));

      if (dead == 0) [[likely]]
        continue;

      if (dead & 1) respawn[count++] = i;
      if (dead & 2) respawn[count++] = i + 1;
      if (dead & 4) respawn[count++] = i + 2;
      if (dead & 8) respawn[count++] = i + 3;
    }

    auto* noalias random = _random.data();

    for (auto j = 0uz; j < count; ++j) {
      const auto offset = j * 12;
      random[offset]      = rng(_radius_range);
      random[offset + 1]  = rng(_angle_range);
      random[offset + 2]  = rng(_spawn_x_range);
      random[offset + 3]  = rng(_spawn_y_range);
      random[offset + 4]  = rng(_velocity_x_range);
      random[offset + 5]  = rng(_velocity_y_range);
      random[offset + 6]  = rng(_gravity_x_range);
      random[offset + 7]  = rng(_gravity_y_range);
      random[offset + 8]  = rng(_rotation_velocity_range);
      random[offset + 9]  = rng(_rotation_force_range);
      random[offset + 10] = rng(_life_range);
      random[offset + 11] = rng(_scale_range);
    }

    const auto vpx = simde_mm_set1_ps(px);
    const auto vpy = simde_mm_set1_ps(py);

    const auto batch = count & ~3uz;
    for (auto j = 0uz; j < batch; j += 4) {
      const auto r0 = random + j * 12;
      const auto r1 = random + j * 12 + 12;
      const auto r2 = random + j * 12 + 24;
      const auto r3 = random + j * 12 + 36;

      const auto vradius = simde_mm_set_ps(r3[0], r2[0], r1[0], r0[0]);
      const auto vangle = simde_mm_set_ps(r3[1], r2[1], r1[1], r0[1]);
      const auto vspawn_x = simde_mm_set_ps(r3[2], r2[2], r1[2], r0[2]);
      const auto vspawn_y = simde_mm_set_ps(r3[3], r2[3], r1[3], r0[3]);

      simde__m128 vsin, vcos;
      sincos4(vangle, vsin, vcos);

      alignas(16) float result_x[4], result_y[4];
      simde_mm_store_ps(result_x, simde_mm_add_ps(vpx, simde_mm_add_ps(vspawn_x, simde_mm_mul_ps(vradius, vcos))));
      simde_mm_store_ps(result_y, simde_mm_add_ps(vpy, simde_mm_add_ps(vspawn_y, simde_mm_mul_ps(vradius, vsin))));

      alignas(16) float result_angle[4];
      simde_mm_store_ps(result_angle, vangle);

      for (auto k = 0uz; k < 4; ++k) {
        const auto i = respawn[j + k];
        const auto* rk = random + (j + k) * 12;
        xs[i] = result_x[k];
        ys[i] = result_y[k];
        vxs[i] = rk[4];
        vys[i] = rk[5];
        gxs[i] = rk[6];
        gys[i] = rk[7];
        avs[i] = rk[8];
        afs[i] = rk[9];
        lifes[i] = rk[10];
        scales[i] = rk[11];
        angles[i] = result_angle[k];
      }
    }

    for (auto j = batch; j < count; ++j) {
      const auto i = respawn[j];
      const auto* rj = random + j * 12;
      const auto r = rj[0];
      const auto a = rj[1];

      float sa, ca;
      sincos(a, sa, ca);

      xs[i] = px + rj[2] + r * ca;
      ys[i] = py + rj[3] + r * sa;
      vxs[i] = rj[4];
      vys[i] = rj[5];
      gxs[i] = rj[6];
      gys[i] = rj[7];
      avs[i] = rj[8];
      afs[i] = rj[9];
      lifes[i] = rj[10];
      scales[i] = rj[11];
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

  auto visible = 0uz;

  const auto vzero = simde_mm_setzero_ps();
  const auto vone = simde_mm_set1_ps(1.f);
  const auto vvpx = simde_mm_set1_ps(viewport.x);
  const auto vvpy = simde_mm_set1_ps(viewport.y);
  const auto vvw = simde_mm_set1_ps(vw);
  const auto vvh = simde_mm_set1_ps(vh);
  const auto vextent = simde_mm_set1_ps(extent);
  const auto vhw = simde_mm_set1_ps(hw);
  const auto vhh = simde_mm_set1_ps(hh);

  for (auto i = 0uz; i < n; i += 4) {
    const auto vlife = simde_mm_loadu_ps(lifes + i);
    const auto alive = simde_mm_cmpgt_ps(vlife, vzero);

    if (simde_mm_movemask_ps(alive) == 0) [[unlikely]]
      continue;

    const auto vsc = simde_mm_loadu_ps(scales + i);
    const auto vse = simde_mm_mul_ps(vextent, vsc);
    const auto vpx = simde_mm_sub_ps(simde_mm_loadu_ps(xs + i), vvpx);
    const auto vpy = simde_mm_sub_ps(simde_mm_loadu_ps(ys + i), vvpy);

    const auto onscreen = simde_mm_and_ps(
      simde_mm_and_ps(simde_mm_cmple_ps(simde_mm_sub_ps(vpx, vse), vvw),
                      simde_mm_cmpge_ps(simde_mm_add_ps(vpx, vse), vzero)),
      simde_mm_and_ps(simde_mm_cmple_ps(simde_mm_sub_ps(vpy, vse), vvh),
                      simde_mm_cmpge_ps(simde_mm_add_ps(vpy, vse), vzero))
    );

    const auto mask = simde_mm_movemask_ps(simde_mm_and_ps(alive, onscreen));

    if (mask == 0) [[unlikely]]
      continue;

    const auto valpha = simde_mm_min_ps(vlife, vone);
    const auto vshw = simde_mm_mul_ps(vhw, vsc);
    const auto vshh = simde_mm_mul_ps(vhh, vsc);

    simde__m128 vsin, vcos;
    sincos4(simde_mm_loadu_ps(angles + i), vsin, vcos);

    const auto vdx0 = simde_mm_add_ps(simde_mm_mul_ps(simde_mm_sub_ps(vzero, vshw), vcos), simde_mm_mul_ps(vshh, vsin));
    const auto vdy0 = simde_mm_sub_ps(simde_mm_mul_ps(simde_mm_sub_ps(vzero, vshw), vsin), simde_mm_mul_ps(vshh, vcos));
    const auto vdx1 = simde_mm_add_ps(simde_mm_mul_ps(vshw, vcos), simde_mm_mul_ps(vshh, vsin));
    const auto vdy1 = simde_mm_sub_ps(simde_mm_mul_ps(vshw, vsin), simde_mm_mul_ps(vshh, vcos));

    alignas(16) float px[4], py[4], alpha[4];
    alignas(16) float dx0[4], dy0[4], dx1[4], dy1[4];
    simde_mm_store_ps(px, vpx);
    simde_mm_store_ps(py, vpy);
    simde_mm_store_ps(alpha, valpha);
    simde_mm_store_ps(dx0, vdx0);
    simde_mm_store_ps(dy0, vdy0);
    simde_mm_store_ps(dx1, vdx1);
    simde_mm_store_ps(dy1, vdy1);

    const auto voffset = simde_mm_set_epi32(0, 2, 1, 0);

    if (mask == 0xF) [[likely]] {
      for (auto j = 0; j < 4; ++j) {
        const auto low0 = simde_mm_set_ps(1.f, 1.f, py[j] + dy0[j], px[j] + dx0[j]);
        const auto low1 = simde_mm_set_ps(1.f, 1.f, py[j] + dy1[j], px[j] + dx1[j]);
        const auto low2 = simde_mm_set_ps(1.f, 1.f, py[j] - dy0[j], px[j] - dx0[j]);
        const auto low3 = simde_mm_set_ps(1.f, 1.f, py[j] - dy1[j], px[j] - dx1[j]);
        const auto high0 = simde_mm_set_ps(.0f, .0f, alpha[j], 1.f);
        const auto high1 = simde_mm_set_ps(.0f, 1.f, alpha[j], 1.f);
        const auto high2 = simde_mm_set_ps(1.f, 1.f, alpha[j], 1.f);
        const auto high3 = simde_mm_set_ps(1.f, .0f, alpha[j], 1.f);

        auto* noalias target = reinterpret_cast<float*>(vertices + visible * 4);
        simde_mm_storeu_ps(target, low0);
        simde_mm_storeu_ps(target + 4, high0);
        simde_mm_storeu_ps(target + 8, low1);
        simde_mm_storeu_ps(target + 12, high1);
        simde_mm_storeu_ps(target + 16, low2);
        simde_mm_storeu_ps(target + 20, high2);
        simde_mm_storeu_ps(target + 24, low3);
        simde_mm_storeu_ps(target + 28, high3);

        const auto vbase = simde_mm_set1_epi32(static_cast<int>(visible * 4));
        auto* noalias ix = indices + visible * 6;
        simde_mm_storeu_si128(reinterpret_cast<simde__m128i*>(ix), simde_mm_add_epi32(vbase, voffset));
        ix[4] = static_cast<int>(visible * 4) + 2;
        ix[5] = static_cast<int>(visible * 4) + 3;

        ++visible;
      }
    } else [[unlikely]] {
      if (mask & 1) {
        auto* noalias target = reinterpret_cast<float*>(vertices + visible * 4);
        simde_mm_storeu_ps(target, simde_mm_set_ps(1.f, 1.f, py[0] + dy0[0], px[0] + dx0[0]));
        simde_mm_storeu_ps(target + 4, simde_mm_set_ps(.0f, .0f, alpha[0], 1.f));
        simde_mm_storeu_ps(target + 8, simde_mm_set_ps(1.f, 1.f, py[0] + dy1[0], px[0] + dx1[0]));
        simde_mm_storeu_ps(target + 12, simde_mm_set_ps(.0f, 1.f, alpha[0], 1.f));
        simde_mm_storeu_ps(target + 16, simde_mm_set_ps(1.f, 1.f, py[0] - dy0[0], px[0] - dx0[0]));
        simde_mm_storeu_ps(target + 20, simde_mm_set_ps(1.f, 1.f, alpha[0], 1.f));
        simde_mm_storeu_ps(target + 24, simde_mm_set_ps(1.f, 1.f, py[0] - dy1[0], px[0] - dx1[0]));
        simde_mm_storeu_ps(target + 28, simde_mm_set_ps(1.f, .0f, alpha[0], 1.f));

        const auto vbase = simde_mm_set1_epi32(static_cast<int>(visible * 4));
        auto* noalias ix = indices + visible * 6;
        simde_mm_storeu_si128(reinterpret_cast<simde__m128i*>(ix), simde_mm_add_epi32(vbase, voffset));
        ix[4] = static_cast<int>(visible * 4) + 2;
        ix[5] = static_cast<int>(visible * 4) + 3;

        ++visible;
      }

      if (mask & 2) {
        auto* noalias target = reinterpret_cast<float*>(vertices + visible * 4);
        simde_mm_storeu_ps(target,      simde_mm_set_ps(1.f, 1.f, py[1] + dy0[1], px[1] + dx0[1]));
        simde_mm_storeu_ps(target + 4,  simde_mm_set_ps(.0f, .0f, alpha[1], 1.f));
        simde_mm_storeu_ps(target + 8,  simde_mm_set_ps(1.f, 1.f, py[1] + dy1[1], px[1] + dx1[1]));
        simde_mm_storeu_ps(target + 12, simde_mm_set_ps(.0f, 1.f, alpha[1], 1.f));
        simde_mm_storeu_ps(target + 16, simde_mm_set_ps(1.f, 1.f, py[1] - dy0[1], px[1] - dx0[1]));
        simde_mm_storeu_ps(target + 20, simde_mm_set_ps(1.f, 1.f, alpha[1], 1.f));
        simde_mm_storeu_ps(target + 24, simde_mm_set_ps(1.f, 1.f, py[1] - dy1[1], px[1] - dx1[1]));
        simde_mm_storeu_ps(target + 28, simde_mm_set_ps(1.f, .0f, alpha[1], 1.f));

        const auto vbase = simde_mm_set1_epi32(static_cast<int>(visible * 4));
        auto* noalias ix = indices + visible * 6;
        simde_mm_storeu_si128(reinterpret_cast<simde__m128i*>(ix), simde_mm_add_epi32(vbase, voffset));
        ix[4] = static_cast<int>(visible * 4) + 2;
        ix[5] = static_cast<int>(visible * 4) + 3;

        ++visible;
      }

      if (mask & 4) {
        auto* noalias target = reinterpret_cast<float*>(vertices + visible * 4);
        simde_mm_storeu_ps(target,      simde_mm_set_ps(1.f, 1.f, py[2] + dy0[2], px[2] + dx0[2]));
        simde_mm_storeu_ps(target + 4,  simde_mm_set_ps(.0f, .0f, alpha[2], 1.f));
        simde_mm_storeu_ps(target + 8,  simde_mm_set_ps(1.f, 1.f, py[2] + dy1[2], px[2] + dx1[2]));
        simde_mm_storeu_ps(target + 12, simde_mm_set_ps(.0f, 1.f, alpha[2], 1.f));
        simde_mm_storeu_ps(target + 16, simde_mm_set_ps(1.f, 1.f, py[2] - dy0[2], px[2] - dx0[2]));
        simde_mm_storeu_ps(target + 20, simde_mm_set_ps(1.f, 1.f, alpha[2], 1.f));
        simde_mm_storeu_ps(target + 24, simde_mm_set_ps(1.f, 1.f, py[2] - dy1[2], px[2] - dx1[2]));
        simde_mm_storeu_ps(target + 28, simde_mm_set_ps(1.f, .0f, alpha[2], 1.f));

        const auto vbase = simde_mm_set1_epi32(static_cast<int>(visible * 4));
        auto* noalias ix = indices + visible * 6;
        simde_mm_storeu_si128(reinterpret_cast<simde__m128i*>(ix), simde_mm_add_epi32(vbase, voffset));
        ix[4] = static_cast<int>(visible * 4) + 2;
        ix[5] = static_cast<int>(visible * 4) + 3;

        ++visible;
      }

      if (mask & 8) {
        auto* noalias target = reinterpret_cast<float*>(vertices + visible * 4);
        simde_mm_storeu_ps(target,      simde_mm_set_ps(1.f, 1.f, py[3] + dy0[3], px[3] + dx0[3]));
        simde_mm_storeu_ps(target + 4,  simde_mm_set_ps(.0f, .0f, alpha[3], 1.f));
        simde_mm_storeu_ps(target + 8,  simde_mm_set_ps(1.f, 1.f, py[3] + dy1[3], px[3] + dx1[3]));
        simde_mm_storeu_ps(target + 12, simde_mm_set_ps(.0f, 1.f, alpha[3], 1.f));
        simde_mm_storeu_ps(target + 16, simde_mm_set_ps(1.f, 1.f, py[3] - dy0[3], px[3] - dx0[3]));
        simde_mm_storeu_ps(target + 20, simde_mm_set_ps(1.f, 1.f, alpha[3], 1.f));
        simde_mm_storeu_ps(target + 24, simde_mm_set_ps(1.f, 1.f, py[3] - dy1[3], px[3] - dx1[3]));
        simde_mm_storeu_ps(target + 28, simde_mm_set_ps(1.f, .0f, alpha[3], 1.f));

        const auto vbase = simde_mm_set1_epi32(static_cast<int>(visible * 4));
        auto* noalias ix = indices + visible * 6;
        simde_mm_storeu_si128(reinterpret_cast<simde__m128i*>(ix), simde_mm_add_epi32(vbase, voffset));
        ix[4] = static_cast<int>(visible * 4) + 2;
        ix[5] = static_cast<int>(visible * 4) + 3;

        ++visible;
      }
    }
  }

  SDL_RenderGeometry(
    renderer,
    static_cast<SDL_Texture*>(*_texture),
    vertices,
    static_cast<int>(visible * 4),
    indices,
    static_cast<int>(visible * 6));
}

void particle::wire() {
  metatable(L, "Particle", particle_index, particle_newindex);
}
