static bool on_event(void *userdata, SDL_Event *event) {
  auto *self = static_cast<overlay *>(userdata);
  switch (event->type) {
    case SDL_EVENT_TEXT_INPUT:
      // std::println("{}", event->text.text);
      break;

    default:
      break;
  }

  return true;
}

static int overlay_label(lua_State *state) {
  auto *self = *static_cast<overlay **>(luaL_checkudata(state, 1, "Overlay"));
  const std::string_view font = luaL_checkstring(state, 2);
  const std::string_view text = luaL_checkstring(state, 3);
  const auto x = static_cast<float>(luaL_checknumber(state, 4));
  const auto y = static_cast<float>(luaL_checknumber(state, 5));

  if (!lua_istable(state, 6)) [[likely]] {
    self->render_label(font, text, x, y);
    return 0;
  }

  std::array<glypheffect, 256> effects{};

  auto length = 0uz;

  lua_pushnil(state);
  while (lua_next(state, 6) != 0) {
    if (!lua_isnumber(state, -2) || !lua_istable(state, -1)) [[unlikely]] {
      lua_pop(state, 1);
      continue;
    }

    const auto index = static_cast<std::size_t>(lua_tointeger(state, -2)) - 1;

    if (index >= effects.size()) {
      lua_pop(state, 1);
      continue;
    }

    auto &effect = effects[index];

    lua_getfield(state, -1, "x_offset");
    if (lua_isnumber(state, -1))
      effect.x_offset = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);

    lua_getfield(state, -1, "y_offset");
    if (lua_isnumber(state, -1))
      effect.y_offset = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);

    lua_getfield(state, -1, "scale");
    if (lua_isnumber(state, -1))
      effect.scale = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);

    lua_getfield(state, -1, "angle");
    if (lua_isnumber(state, -1))
      effect.angle = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);

    lua_getfield(state, -1, "alpha");
    if (lua_isnumber(state, -1))
      effect.alpha = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);

    lua_getfield(state, -1, "r");
    if (lua_isnumber(state, -1))
      effect.r = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);

    lua_getfield(state, -1, "g");
    if (lua_isnumber(state, -1))
      effect.g = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);

    lua_getfield(state, -1, "b");
    if (lua_isnumber(state, -1))
      effect.b = static_cast<float>(lua_tonumber(state, -1));
    lua_pop(state, 1);

    if (index >= length) length = index + 1;

    lua_pop(state, 1);
  }

  self->render_label(font, text, x, y, std::span{effects.data(), length});
  return 0;
}

namespace {
  namespace property {
    constexpr auto label = "label"_hs;
  }

  int _label_ref = LUA_NOREF;
}

static int overlay_index(lua_State *state) {
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  if (id == property::label) {
    lua_rawgeti(state, LUA_REGISTRYINDEX, _label_ref);
    return 1;
  }

  return lua_pushnil(state), 1;
}

overlay::overlay(std::string_view name) {
  const auto filename = std::format("overlays/{}.lua", name);
  const auto buffer = io::read(filename);
  const auto chunk = std::format("@{}", filename);
  compile(L, buffer, chunk);

  pcall(L, 0, 1);

  lua_getfield(L, -1, "fonts");
  if (lua_istable(L, -1)) {
    const auto count = static_cast<int>(lua_objlen(L, -1));

    for (int i = 1; i <= count; ++i) {
      lua_rawgeti(L, -1, i);

      if (lua_isstring(L, -1)) {
        depot->font.get(lua_tostring(L, -1));
      }

      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);

  _ref = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _ref);

  lua_getfield(L, -1, "on_loop");
  _on_loop = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_paint");
  _on_paint = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_pop(L, 1);

  auto **m = static_cast<overlay **>(lua_newuserdata(L, sizeof(overlay *)));
  *m = this;
  luaL_getmetatable(L, "Overlay");
  lua_setmetatable(L, -2);
  _userdata_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  // SDL_AddEventWatch(on_event, this);
  // SDL_StartTextInput(SDL_GetRenderWindow(renderer));
}

overlay::~overlay() {
  // SDL_StopTextInput(SDL_GetRenderWindow(renderer));
  // SDL_RemoveEventWatch(on_event, this);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_paint);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_loop);
  luaL_unref(L, LUA_REGISTRYINDEX, _ref);
  luaL_unref(L, LUA_REGISTRYINDEX, _userdata_ref);
}

void overlay::wire() {
  lua_pushcfunction(L, overlay_label);
  _label_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  metatable(L, "Overlay", overlay_index);
}

void overlay::set_foreground(std::string_view name) {
  if (_foreground)
    _foreground->disappear();

  _foreground = std::make_unique<foreground>(name);
  _foreground->expose();
  _foreground->appear();
}

void overlay::update(float delta) {
  if (_foreground)
    _foreground->update(delta);

  if (_on_loop != LUA_NOREF) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_loop);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _ref);
    lua_pushnumber(L, static_cast<lua_Number>(delta));
    pcall(L, 2, 0);
  }
}

void overlay::draw() {
  if (_foreground)
    _foreground->draw();

  if (_on_paint != LUA_NOREF) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_paint);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _ref);
    pcall(L, 1, 0);
  }
}

void overlay::expose() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _userdata_ref);
  lua_setglobal(L, "overlay");
}

void overlay::render_label(std::string_view family, std::string_view text, float x, float y) {
  auto *f = depot->font.get(family);
  f->draw(text, x, y);
}

void overlay::render_label(std::string_view family, std::string_view text, float x, float y, std::span<const glypheffect> effects) {
  auto *f = depot->font.get(family);
  f->draw(text, x, y, effects);
}
