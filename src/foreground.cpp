namespace {
namespace lookup {
  constexpr auto draw = "draw"_hs;
}

int paint = LUA_NOREF;

static int render(lua_State *state) {
  auto *self = *static_cast<foreground **>(luaL_checkudata(state, 1, "Foreground"));
  if (!self->_texture) [[unlikely]]
    return 0;
  luaL_checktype(state, 2, LUA_TTABLE);
  const auto count = static_cast<int>(luaL_checkinteger(state, 3));

  if (count <= 0 || count % 6 != 0) [[unlikely]]
    return 0;

  auto &vertices = self->_vertices;
  auto &indices = self->_indices;

  vertices.clear();
  indices.clear();

  const auto quads = count / 6;

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
    const auto alpha = static_cast<float>(lua_tonumber(state, -1)) / 255.f;

    lua_pop(state, 6);

    if (alpha <= .0f) [[unlikely]]
      continue;

    const SDL_FColor color{1.f, 1.f, 1.f, alpha};
    const auto base = static_cast<int32_t>(vertices.size());
    const auto hw = w * .5f;
    const auto hh = h * .5f;
    const auto cx = x + hw;
    const auto cy = y + hh;

    auto sa = .0f, ca = 1.f;
    if (angle != .0f)
      sincos(to_radians(angle), sa, ca);

    const auto dx0 = -hw * ca + hh * sa;
    const auto dy0 = -hw * sa - hh * ca;
    const auto dx1 = hw * ca + hh * sa;
    const auto dy1 = hw * sa - hh * ca;

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

  if (vertices.empty()) [[unlikely]]
    return 0;

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

static int index(lua_State *state) {
  auto *self = *static_cast<foreground **>(luaL_checkudata(state, 1, "Foreground"));
  const std::string_view key = luaL_checkstring(state, 2);
  const auto id = entt::hashed_string{key.data(), key.size()};

  if (id == lookup::draw) {
    lua_rawgeti(state, LUA_REGISTRYINDEX, paint);
    return 1;
  }

  lua_rawgeti(state, LUA_REGISTRYINDEX, self->_table);
  lua_getfield(state, -1, key.data());
  if (!lua_isnil(state, -1)) [[likely]] {
    lua_remove(state, -2);
    return 1;
  }
  lua_pop(state, 1);

  std::array<char, 64> buffer;
  const auto length = std::min(key.size(), std::size_t{60});
  buffer[0] = 'o';
  buffer[1] = 'n';
  buffer[2] = '_';
  std::memcpy(buffer.data() + 3, key.data(), length);
  buffer[3 + length] = '\0';

  lua_getfield(state, -1, buffer.data());
  lua_remove(state, -2);
  if (!lua_isnil(state, -1))
    return 1;
  lua_pop(state, 1);

  return lua_pushnil(state), 1;
}
}

foreground::foreground(std::string_view name) {
  _vertices.reserve(2048);
  _indices.reserve(3072);

  const auto filename = std::format("foregrounds/{}.lua", name);
  const auto buffer = io::read(filename);
  const auto chunk = std::format("@{}", filename);
  if (luaL_loadbuffer(L, reinterpret_cast<const char*>(buffer.data()), buffer.size(), chunk.c_str()) != LUA_OK) [[unlikely]] {
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
        auto *font = depot->font.get(family);

        auto **m = static_cast<font **>(lua_newuserdata(L, sizeof(font *)));
        *m = font;
        luaL_getmetatable(L, "Font");
        lua_setmetatable(L, -2);

        lua_setfield(L, -4, family);
      }

      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);

  _table = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _table);

  lua_getfield(L, -1, "on_appear");
  _on_appear = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_disappear");
  _on_disappear = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_loop");
  _on_loop = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_paint");
  _on_paint = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_pop(L, 1);

  auto **m = static_cast<foreground **>(lua_newuserdata(L, sizeof(foreground *)));
  *m = this;
  luaL_getmetatable(L, "Foreground");
  lua_setmetatable(L, -2);
  _userdata = luaL_ref(L, LUA_REGISTRYINDEX);
}

foreground::~foreground() {
  luaL_unref(L, LUA_REGISTRYINDEX, _on_paint);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_loop);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_disappear);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_appear);
  luaL_unref(L, LUA_REGISTRYINDEX, _table);
  luaL_unref(L, LUA_REGISTRYINDEX, _userdata);
}

void foreground::wire() {
  lua_pushcfunction(L, render);
  paint = luaL_ref(L, LUA_REGISTRYINDEX);

  luaL_newmetatable(L, "Foreground");
  lua_pushliteral(L, "Foreground");
  lua_setfield(L, -2, "__name");

  lua_pushcfunction(L, index);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);
}

void foreground::appear() {
  if (_visible) [[unlikely]]
    return;

  _visible = true;

  if (_on_appear != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_appear);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
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

  if (_on_disappear != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_disappear);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
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

  if (_on_loop != LUA_NOREF) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_loop);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
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

  if (_on_paint != LUA_NOREF) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_paint);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _userdata);
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
