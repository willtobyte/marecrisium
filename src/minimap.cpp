#include "minimap.hpp"

namespace {
namespace property {
  constexpr auto visible = "visible"_hs;
}
}

static int minimap_index(lua_State *state) {
  auto *self = *static_cast<minimap **>(lua_touserdata(state, 1));
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  if (id == property::visible) {
    lua_pushboolean(state, self->_visible ? 1 : 0);
    return 1;
  }

  return lua_pushnil(state), 1;
}

static int minimap_newindex(lua_State *state) {
  auto *self = *static_cast<minimap **>(lua_touserdata(state, 1));
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
    , _solid(solid)
    , _passable(passable)
    , _empty(empty)
    , _player(player)
    , _entity(entity)
    , _pixels(static_cast<size_t>(SIDE * SIDE)) {
  _texture.reset(SDL_CreateTexture(
    renderer,
    SDL_PIXELFORMAT_RGBA32,
    SDL_TEXTUREACCESS_STREAMING,
    SIDE, SIDE));

  SDL_SetTextureScaleMode(_texture.get(), SDL_SCALEMODE_NEAREST);
}

void minimap::rebuild() noexcept {
  const auto tw = _tilemap->_width;
  const auto th = _tilemap->_height;
  const auto ts = _tilemap->_size;
  const auto &collision = _tilemap->_collision;

  const auto cx = static_cast<int32_t>(_position_x / ts);
  const auto cy = static_cast<int32_t>(_position_y / ts);

  auto *noalias pixels = _pixels.data();

  for (int32_t dy = -RADIUS; dy <= RADIUS; ++dy) {
    const auto ty = cy + dy;
    for (int32_t dx = -RADIUS; dx <= RADIUS; ++dx) {
      const auto tx = cx + dx;
      const auto index = static_cast<size_t>((dy + RADIUS) * SIDE + (dx + RADIUS));

      if (tx < 0 || tx >= tw || ty < 0 || ty >= th) [[unlikely]] {
        pixels[index] = static_cast<uint32_t>(_empty);
        continue;
      }

      const auto tile = collision[static_cast<size_t>(ty * tw + tx)];
      pixels[index] = tile != 0 ? static_cast<uint32_t>(_solid) : static_cast<uint32_t>(_passable);
    }
  }

  for (auto &&[en, op, tf] :
       _registry->view<const objectproxy, const transform>(entt::exclude<dormant>).each()) {
    if (op.handle == LUA_NOREF) [[unlikely]]
      continue;

    if (op.kind == "player"_hs) [[unlikely]] {
      _position_x = tf.x;
      _position_y = tf.y;
      continue;
    }

    const auto ex = static_cast<int32_t>(tf.x / ts) - cx + RADIUS;
    const auto ey = static_cast<int32_t>(tf.y / ts) - cy + RADIUS;

    if (ex < 0 || ex >= SIDE || ey < 0 || ey >= SIDE) [[likely]]
      continue;

    pixels[static_cast<size_t>(ey * SIDE + ex)] = static_cast<uint32_t>(_entity);
  }

  pixels[static_cast<size_t>(RADIUS * SIDE + RADIUS)] = static_cast<uint32_t>(_player);
}

void minimap::draw() noexcept {
  if (!_visible)
    return;

  rebuild();

  void *raw = nullptr;
  int pitch;
  if (!SDL_LockTexture(_texture.get(), nullptr, &raw, &pitch)) [[unlikely]]
    return;

  std::memcpy(raw, _pixels.data(), static_cast<size_t>(pitch) * SIDE);

  SDL_UnlockTexture(_texture.get());

  static const SDL_FRect target{
    (viewport.width - SIZE) * .5f,
    (viewport.height - SIZE) * .5f,
    SIZE, SIZE
  };

  SDL_RenderTexture(renderer, _texture.get(), nullptr, &target);
}

void minimap::wire() {
  metatable(L, "Minimap", minimap_index, minimap_newindex);
}
