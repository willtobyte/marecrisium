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

namespace {
  namespace property {
    constexpr auto foreground = "foreground"_hs;
  }
}

static int overlay_newindex(lua_State *state) {
  auto *self = *static_cast<overlay **>(luaL_checkudata(state, 1, "Overlay"));
  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};

  if (id == property::foreground) [[likely]] {
    if (lua_isnoneornil(state, 3)) {
      self->dismiss();
      return 0;
    }

    self->set_foreground(luaL_checkstring(state, 3));
    return 0;
  }

  return 0;
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
  metatable(L, "Overlay", nullptr, overlay_newindex);
}

void overlay::set_foreground(std::string_view name) {
  if (_foreground)
    _foreground->disappear();

  const auto key = entt::hashed_string{name.data(), name.size()};
  const auto [it, inserted] = _foregrounds.try_emplace(key, nullptr);
  if (inserted)
    it->second = std::make_unique<foreground>(name);

  _foreground = it->second.get();
  _foreground->expose();
  _foreground->appear();
}

void overlay::dismiss() {
  if (!_foreground)
    return;

  _foreground->disappear();
  _foreground = nullptr;
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
