#include "minimap.hpp"

namespace {
namespace property {
  constexpr auto visible = "visible"_hs;
}
}

static int minimap_index(lua_State *state) {
  auto *self = *static_cast<minimap **>(luaL_checkudata(state, 1, "Minimap"));
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  if (id == property::visible) {
    lua_pushboolean(state, self->_visible ? 1 : 0);
    return 1;
  }

  return lua_pushnil(state), 1;
}

static int minimap_newindex(lua_State *state) {
  auto *self = *static_cast<minimap **>(luaL_checkudata(state, 1, "Minimap"));
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  if (id == property::visible)
    self->_visible = lua_toboolean(state, 3) != 0;

  return 0;
}

minimap::minimap(const tilemap &tilemap, entt::registry &registry,
                 color solid, color passable, color empty,
                 color player, color entity)
    : _tilemap(&tilemap)
    , _registry(&registry)
    , _solid(static_cast<uint32_t>(solid))
    , _passable(static_cast<uint32_t>(passable))
    , _empty(static_cast<uint32_t>(empty))
    , _player(static_cast<uint32_t>(player))
    , _entity(static_cast<uint32_t>(entity))
    , _pixels(static_cast<size_t>(SIDE * SIDE)) {
  _texture.reset(SDL_CreateTexture(
    renderer,
    SDL_PIXELFORMAT_RGBA32,
    SDL_TEXTUREACCESS_STATIC,
    SIDE, SIDE));

  SDL_SetTextureScaleMode(_texture.get(), SDL_SCALEMODE_NEAREST);
}

void minimap::draw() {
  if (!_visible)
    return;

  const auto tw = _tilemap->_width;
  const auto th = _tilemap->_height;
  const auto ts = _tilemap->_size;
  const auto &collision = _tilemap->_collision;

  _positions.clear();

  const auto view = _registry->view<const scriptable, const transform>(entt::exclude<dormant>);
  _positions.reserve(view.size_hint());

  for (auto &&[en, op, tf] : view.each()) {
    if (op.handle == LUA_NOREF) [[unlikely]]
      continue;

    if (op.kind == "player"_hs) [[unlikely]] {
      _position_x = tf.x;
      _position_y = tf.y;
      continue;
    }

    _positions.emplace_back(tf.x, tf.y);
  }

  const auto cx = static_cast<int32_t>(_position_x / ts);
  const auto cy = static_cast<int32_t>(_position_y / ts);

  static const SDL_FRect target{
    (viewport.width - SIZE) * .5f,
    (viewport.height - SIZE) * .5f,
    SIZE, SIZE
  };

  const snapshot current{cx, cy, _positions.size()};

  if (current == std::exchange(_previous, current)) [[likely]] {
    SDL_RenderTexture(renderer, _texture.get(), nullptr, &target);
    return;
  }

  auto *noalias pixels = _pixels.data();
  const auto* noalias coll = collision.data();

  for (int32_t dy = -RADIUS; dy <= RADIUS; ++dy) {
    const auto ty = cy + dy;
    const auto row = static_cast<size_t>((dy + RADIUS) * SIDE);

    std::fill_n(pixels + row, SIDE, _empty);

    if (ty < 0 || ty >= th) [[unlikely]]
      continue;

    const auto base = static_cast<size_t>(ty * tw);
    const auto dx_lo = std::max(static_cast<int32_t>(-RADIUS), -cx);
    const auto dx_hi = std::min(static_cast<int32_t>(RADIUS), tw - 1 - cx);

    for (auto dx = dx_lo; dx <= dx_hi; ++dx) {
      pixels[row + static_cast<size_t>(dx + RADIUS)] =
        coll[base + static_cast<size_t>(cx + dx)] != 0 ? _solid : _passable;
    }
  }

  for (const auto &[x, y] : _positions) {
    const auto ex = static_cast<int32_t>(x / ts) - cx + RADIUS;
    const auto ey = static_cast<int32_t>(y / ts) - cy + RADIUS;

    if (ex < 0 || ex >= SIDE || ey < 0 || ey >= SIDE) [[likely]]
      continue;

    pixels[static_cast<size_t>(ey * SIDE + ex)] = _entity;
  }

  pixels[static_cast<size_t>(RADIUS * SIDE + RADIUS)] = _player;

  SDL_UpdateTexture(_texture.get(), nullptr, pixels, SIDE * SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_RGBA32));

  SDL_RenderTexture(renderer, _texture.get(), nullptr, &target);
}

void minimap::wire() {
  metatable(L, "Minimap", minimap_index, minimap_newindex);
}
