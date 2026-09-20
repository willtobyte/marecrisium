namespace {
template <typename T>
void number(lua_State *state, int table, const char *field, T &value, T fallback = {}) {
  lua_getfield(state, table, field);

  int valid;
  const auto result = lua_tonumberx(state, -1, &valid);

  value = valid ? static_cast<T>(result) : fallback;

  lua_pop(state, 1);
}

template <typename T>
void number(lua_State *state, int table, const char *field, T &value, T fallback, T minimum, T maximum) {
  lua_getfield(state, table, field);

  int valid;
  const auto result = lua_tonumberx(state, -1, &valid);

  value = valid ? std::clamp(static_cast<T>(result), minimum, maximum) : fallback;

  lua_pop(state, 1);
}

int create(lua_State *state) {
  std::size_t length;
  const auto *data = luaL_checklstring(state, 1, &length);
  const auto family = std::string_view{data, length};
  const auto x = static_cast<float>(luaL_checknumber(state, 2));
  const auto y = static_cast<float>(luaL_checknumber(state, 3));
  const auto w = static_cast<float>(luaL_checknumber(state, 4));
  const auto h = static_cast<float>(luaL_checknumber(state, 5));
  auto *memory = static_cast<class label *>(lua_newuserdata(state, sizeof(class label)));

  new (memory) label(depot->get<font>(family), x, y, w, h);
  luaL_getmetatable(state, "Label");
  lua_setmetatable(state, -2);

  return 1;
}

int paint(lua_State *state) {
  auto *self = static_cast<class label *>(luaL_checkudata(state, 1, "Label"));
  std::size_t length;
  const auto *data = luaL_checklstring(state, 2, &length);
  const auto text = std::string_view{data, length};

  if (!lua_istable(state, 3)) [[likely]] {
    self->draw(text);

    return 0;
  }

  static std::array<glypheffect, 256> effects;
  std::array<uint64_t, 4> active{};
  auto count = 0uz;

  for (lua_pushnil(state); lua_next(state, 3) != 0; lua_pop(state, 1)) {
    const auto raw = lua_tointeger(state, -2);
    const auto valid = raw > 0 && raw <= static_cast<lua_Integer>(effects.size());
    assert(valid && "glyph effect index must be valid");
    [[assume(valid)]];

    const auto slot = static_cast<std::size_t>(raw) - 1;

    active[slot / 64] |= uint64_t{1} << (slot % 64);

    auto &effect = effects[slot];
    number(state, -1, "x_offset", effect.x_offset, .0f);
    number(state, -1, "y_offset", effect.y_offset, .0f);
    number(state, -1, "scale", effect.scale, 1.f);
    number(state, -1, "angle", effect.angle, .0f);
    number(state, -1, "alpha", effect.alpha, 1.f, .0f, 1.f);
    number(state, -1, "r", effect.r, 1.f, .0f, 1.f);
    number(state, -1, "g", effect.g, 1.f, .0f, 1.f);
    number(state, -1, "b", effect.b, 1.f, .0f, 1.f);

    count = std::max(count, slot + 1);
  }

  self->draw<true>(text, std::span{effects.data(), count}, active);

  return 0;
}
}

label::label(font *font, float x, float y, float w, float h) : _font(font), _x(x), _y(y), _w(w), _h(h) {
}

void label::wire() {
  lua_createtable(L, 0, 2);
  lua_pushliteral(L, "Label");
  lua_setfield(L, -2, "__name");
  lua_pushcfunction(L, paint);
  lua_setfield(L, -2, "draw");
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  lua_setfield(L, LUA_REGISTRYINDEX, "Label");

  lua_createtable(L, 0, 1);
  lua_pushcfunction(L, create);
  lua_setfield(L, -2, "new");
  lua_setglobal(L, "Label");
}

void label::draw(std::string_view text) const {
  _font->draw(text, _x, _y, _w, _h);
}

void label::draw(std::string_view text, std::span<const glypheffect> effects) const {
  draw<false>(text, effects, {});
}

template <bool sparse>
void label::draw(std::string_view text, std::span<const glypheffect> effects, std::span<const uint64_t> active) const {
  if constexpr (sparse) {
    _font->draw<true>(text, _x, _y, _w, _h, effects, active);
  } else {
    _font->draw<false>(text, _x, _y, _w, _h, effects, active);
  }
}
