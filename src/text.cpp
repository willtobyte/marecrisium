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

  const auto properties = SDL_CreateProperties();
  SDL_SetBooleanProperty(properties, SDL_PROP_TEXTINPUT_AUTOCORRECT_BOOLEAN, false);
  SDL_StartTextInputWithProperties(SDL_GetRenderWindow(renderer), properties);
  SDL_DestroyProperties(properties);

  return 0;
}

static int text_index(lua_State *state) {
  cfunction(state, text_on);
  return 1;
}

void text::wire() {
  SDL_AddEventWatch(on_event, nullptr);

  metatable(L, "Text", text_index);

  singleton(L, "Text", "text");
}
