namespace {
namespace lookup {
  constexpr auto visible = "visible"_hs;
}
}

static int index(lua_State *state) {
  auto *self = *static_cast<minimap **>(luaL_checkudata(state, 1, "Minimap"));
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  if (id == lookup::visible) {
    lua_pushboolean(state, self->_visible ? 1 : 0);
    return 1;
  }

  return lua_pushnil(state), 1;
}

static int newindex(lua_State *state) {
  auto *self = *static_cast<minimap **>(luaL_checkudata(state, 1, "Minimap"));
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  if (id == lookup::visible)
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
    , _pixels(static_cast<size_t>(side * side)) {
  _texture.reset(SDL_CreateTexture(
    renderer,
    SDL_PIXELFORMAT_RGBA32,
    SDL_TEXTUREACCESS_STATIC,
    side, side));

  SDL_SetTextureScaleMode(_texture.get(), SDL_SCALEMODE_NEAREST);
}

void minimap::draw() {
  if (!_visible)
    return;

  const auto tw = _tilemap->_width;
  const auto th = _tilemap->_height;
  const auto ts = _tilemap->_size;
  const auto &collision = _tilemap->_collision;

  const auto view = _registry->view<const scriptable, const transform>(entt::exclude<dormant>);

  uint64_t digest = 0;
  for (auto &&[en, op, tf] : view.each()) {
    if (op.handle == LUA_NOREF) [[unlikely]]
      continue;

    if (op.kind == "player"_hs) [[unlikely]] {
      _position_x = tf.x;
      _position_y = tf.y;
      continue;
    }

    const auto ex = static_cast<int32_t>(tf.x / ts);
    const auto ey = static_cast<int32_t>(tf.y / ts);
    const auto packed = (static_cast<uint64_t>(static_cast<uint32_t>(ex)) << 32)
                      |  static_cast<uint64_t>(static_cast<uint32_t>(ey));
    digest = (digest << 6) + (digest >> 2) + packed + 0x9e3779b97f4a7c15ull;
  }

  const auto cx = static_cast<int32_t>(_position_x / ts);
  const auto cy = static_cast<int32_t>(_position_y / ts);

  static const SDL_FRect target{
    (viewport.width - size) * .5f,
    (viewport.height - size) * .5f,
    size, size
  };

  const snapshot current{cx, cy, digest};

  if (current == std::exchange(_previous, current)) [[likely]] {
    SDL_RenderTexture(renderer, _texture.get(), nullptr, &target);
    return;
  }

  auto *noalias pixels = _pixels.data();
  const auto* noalias coll = collision.data();

  [[assume(pixels != nullptr)]];
  [[assume(coll != nullptr)]];

  for (int32_t dy = -radius; dy <= radius; ++dy) {
    const auto ty = cy + dy;
    const auto row = static_cast<size_t>((dy + radius) * side);

    std::fill_n(pixels + row, side, _empty);

    if (ty < 0 || ty >= th) [[unlikely]]
      continue;

    const auto base = static_cast<size_t>(ty * tw);
    const auto lo = std::max(static_cast<int32_t>(-radius), -cx);
    const auto hi = std::min(static_cast<int32_t>(radius), tw - 1 - cx);

    for (auto dx = lo; dx <= hi; ++dx) {
      pixels[row + static_cast<size_t>(dx + radius)] =
        coll[base + static_cast<size_t>(cx + dx)] != 0 ? _solid : _passable;
    }
  }

  for (auto &&[en, op, tf] : view.each()) {
    if (op.handle == LUA_NOREF || op.kind == "player"_hs) [[unlikely]]
      continue;

    const auto ex = static_cast<int32_t>(tf.x / ts) - cx + radius;
    const auto ey = static_cast<int32_t>(tf.y / ts) - cy + radius;

    if (ex < 0 || ex >= side || ey < 0 || ey >= side) [[likely]]
      continue;

    pixels[static_cast<size_t>(ey * side + ex)] = _entity;
  }

  pixels[static_cast<size_t>(radius * side + radius)] = _player;

  SDL_UpdateTexture(_texture.get(), nullptr, pixels, side * SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_RGBA32));

  SDL_RenderTexture(renderer, _texture.get(), nullptr, &target);
}

void minimap::wire() {
  luaL_newmetatable(L, "Minimap");
  lua_pushliteral(L, "Minimap");
  lua_setfield(L, -2, "__name");

  lua_pushcfunction(L, index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, newindex);
  lua_setfield(L, -2, "__newindex");
  lua_pop(L, 1);
}
