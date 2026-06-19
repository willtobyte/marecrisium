namespace {
auto _callback = LUA_NOREF;
}

static bool on_event(void *, SDL_Event *event) {
  if (event->type != SDL_EVENT_TEXT_INPUT) [[likely]]
    return true;

  if (_callback == LUA_NOREF) [[unlikely]]
    return true;

  lua_rawgeti(L, LUA_REGISTRYINDEX, _callback);
  lua_pushstring(L, event->text.text);
  pcall(L, 1, 0);

  return true;
}

static int text_on(lua_State *state) {
  luaL_checktype(state, 1, LUA_TFUNCTION);

  luaL_unref(state, LUA_REGISTRYINDEX, _callback);
  lua_pushvalue(state, 1);
  _callback = luaL_ref(state, LUA_REGISTRYINDEX);

  SDL_StartTextInput(SDL_GetRenderWindow(renderer));

  return 0;
}

static int text_index(lua_State *state) {
  lua_pushcfunction(state, text_on);
  return 1;
}

void text::wire() {
  SDL_SetHint(SDL_HINT_MAC_PRESS_AND_HOLD, "0");
  SDL_AddEventWatch(on_event, nullptr);

  metatable(L, "Text", text_index);

  singleton(L, "Text", "text");
}
