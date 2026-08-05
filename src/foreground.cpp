static int draw_callback(lua_State *state) {
  auto *self = static_cast<foreground *>(lua_touserdata(state, lua_upvalueindex(1)));
  const auto table = lua_istable(state, 2) != 0;
  assert(table && "foreground buffer must be a table");
  [[assume(table)]];
  const auto count = static_cast<int>(luaL_checkinteger(state, 3));
  assert(count > 0 && count % 6 == 0 && "foreground buffer count must be a positive multiple of six");
  [[assume(count > 0 && count % 6 == 0)]];

  auto &vertices = self->_vertices;
  auto &indices = self->_indices;

  vertices.clear();
  indices.clear();

  const auto quads = count / 6;

  assert(quads > 0 && "foreground must contain a quad");
  [[assume(quads > 0)]];

  for (auto quad = 0; quad < quads; ++quad) {
    const auto index = quad * 6;

    lua_rawgeti(state, 2, index + 1);
    lua_rawgeti(state, 2, index + 2);
    lua_rawgeti(state, 2, index + 3);
    lua_rawgeti(state, 2, index + 4);
    lua_rawgeti(state, 2, index + 5);
    lua_rawgeti(state, 2, index + 6);

    const auto x = static_cast<float>(lua_tonumber(state, -6));
    const auto y = static_cast<float>(lua_tonumber(state, -5));
    const auto w = static_cast<float>(lua_tonumber(state, -4));
    const auto h = static_cast<float>(lua_tonumber(state, -3));
    const auto angle = static_cast<float>(lua_tonumber(state, -2));
    const auto alpha = std::clamp(
      static_cast<float>(lua_tonumber(state, -1)), .0f, 255.f) / 255.f;

    lua_pop(state, 6);

    if (alpha <= .0f) [[unlikely]]
      continue;

    const SDL_FColor color{1.f, 1.f, 1.f, alpha};
    const auto base = static_cast<int32_t>(vertices.size());
    const auto hw = w * .5f;
    const auto hh = h * .5f;
    const auto cx = x + hw;
    const auto cy = y + hh;

    auto sine = .0f;
    auto cosine = 1.f;
    if (angle != .0f) {
      const auto radians = angle * (std::numbers::pi_v<float> / 180.f);
      sine = std::sin(radians);
      cosine = std::cos(radians);
    }

    const auto dx0 = -hw * cosine + hh * sine;
    const auto dy0 = -hw * sine - hh * cosine;
    const auto dx1 = hw * cosine + hh * sine;
    const auto dy1 = hw * sine - hh * cosine;

    vertices.emplace_back(SDL_Vertex{{cx + dx0, cy + dy0}, color, {.0f, .0f}});
    vertices.emplace_back(SDL_Vertex{{cx + dx1, cy + dy1}, color, {1.f, .0f}});
    vertices.emplace_back(SDL_Vertex{{cx - dx0, cy - dy0}, color, {1.f, 1.f}});
    vertices.emplace_back(SDL_Vertex{{cx - dx1, cy - dy1}, color, {.0f, 1.f}});

    indices.emplace_back(base);
    indices.emplace_back(base + 1);
    indices.emplace_back(base + 2);
    indices.emplace_back(base);
    indices.emplace_back(base + 2);
    indices.emplace_back(base + 3);
  }

  SDL_RenderGeometry(
    renderer,
    static_cast<SDL_Texture *>(*self->_texture),
    vertices.data(),
    static_cast<int>(vertices.size()),
    indices.data(),
    static_cast<int>(indices.size())
  );

  return 0;
}

foreground::foreground(std::string_view name) {
  _vertices.reserve(2048);
  _indices.reserve(3072);

  const auto chunk = std::format("@foregrounds/{}.lua", name);
  const auto path = std::string_view{chunk}.substr(1);
  const auto source = io::read(path);
  if (luaL_loadbuffer(L, reinterpret_cast<const char*>(source.data()), source.size(), chunk.c_str()) != LUA_OK) [[unlikely]] {
    lua_error(L);
    std::unreachable();
  }

  {
    const auto base = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
    lua_insert(L, base);
    const auto status = lua_pcall(L, 0, 1, base);
    lua_remove(L, base);
    if (status != LUA_OK) [[unlikely]] {
      lua_error(L);
      std::unreachable();
    }
  }

  const auto pp = std::format("foregrounds/{}/pixmap", name);
  if (io::exists(std::format("blobs/{}.png", pp))) {
    _texture = depot->pixmap.get(pp);

    lua_newtable(L);
    lua_pushnumber(L, static_cast<lua_Number>(_texture->width()));
    lua_setfield(L, -2, "width");
    lua_pushnumber(L, static_cast<lua_Number>(_texture->height()));
    lua_setfield(L, -2, "height");
    lua_setfield(L, -2, "pixmap");
  }

  lua_getfield(L, -1, "fonts");
  if (lua_istable(L, -1)) {
    const auto count = static_cast<int>(lua_objlen(L, -1));

    for (int index = 1; index <= count; ++index) {
      lua_rawgeti(L, -1, index);

      if (lua_isstring(L, -1)) [[likely]] {
        const auto *family = lua_tostring(L, -1);
        auto **f = static_cast<font **>(lua_newuserdata(L, sizeof(font *)));
        *f = depot->font.get(family);
        luaL_getmetatable(L, "Font");
        lua_setmetatable(L, -2);

        lua_setfield(L, -4, family);
      }

      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);

  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, draw_callback, 1);
  lua_setfield(L, -2, "draw");
  _table_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _table_ref);

  lua_getfield(L, -1, "on_appear");
  _on_appear_ref = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_disappear");
  _on_disappear_ref = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_loop");
  _on_loop_ref = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_paint");
  _on_paint_ref = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_pop(L, 1);

}

foreground::~foreground() {
  luaL_unref(L, LUA_REGISTRYINDEX, _on_paint_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_loop_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_disappear_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_appear_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, _table_ref);
}

void foreground::appear() {
  if (_visible) [[unlikely]]
    return;

  _visible = true;

  if (_on_appear_ref != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_appear_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table_ref);
    {
      const auto base = lua_gettop(L) - 1;
      lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
      lua_insert(L, base);
      const auto status = lua_pcall(L, 1, 0, base);
      lua_remove(L, base);
      if (status != LUA_OK) [[unlikely]] {
        lua_error(L);
        std::unreachable();
      }
    }
  }
}

void foreground::disappear() {
  if (!_visible)
    return;

  _visible = false;

  if (_on_disappear_ref != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_disappear_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table_ref);
    const auto base = lua_gettop(L) - 1;
    lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
    lua_insert(L, base);
    const auto status = lua_pcall(L, 1, 0, base);
    lua_remove(L, base);
    if (status != LUA_OK) [[unlikely]] {
      lua_error(L);
      std::unreachable();
    }
  }
}

void foreground::update(float delta) {
  if (!_visible) [[unlikely]]
    return;

  if (_on_loop_ref != LUA_NOREF) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_loop_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table_ref);
    lua_pushnumber(L, static_cast<lua_Number>(delta));
    {
      const auto base = lua_gettop(L) - 2;
      lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
      lua_insert(L, base);
      const auto status = lua_pcall(L, 2, 0, base);
      lua_remove(L, base);
      if (status != LUA_OK) [[unlikely]] {
        lua_error(L);
        std::unreachable();
      }
    }
  }
}

void foreground::draw() {
  if (!_visible) [[unlikely]]
    return;

  if (_on_paint_ref != LUA_NOREF) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_paint_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table_ref);
    {
      const auto base = lua_gettop(L) - 1;
      lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
      lua_insert(L, base);
      const auto status = lua_pcall(L, 1, 0, base);
      lua_remove(L, base);
      if (status != LUA_OK) [[unlikely]] {
        lua_error(L);
        std::unreachable();
      }
    }
  }
}
