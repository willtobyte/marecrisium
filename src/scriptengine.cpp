static int loader_callback(lua_State *state) {
  const auto chunk = std::format("@scripts/{}.lua", luaL_checkstring(state, 1));
  const auto path = std::string_view{chunk}.substr(1);
  const auto source = io::read(path);

  if (luaL_loadbuffer(state, reinterpret_cast<const char *>(source.data()), source.size(), chunk.c_str()) != LUA_OK)
    return lua_error(state);

  return 1;
}

void scriptengine::run() {
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "loaders");

  const auto length = static_cast<int>(lua_objlen(L, -1));
  lua_pushcfunction(L, loader_callback);
  lua_rawseti(L, -2, length + 1);

  lua_pop(L, 2);

  achievement::wire();
  cassette::wire();
  font::wire();
  gamepad::wire();
  keyboard::wire();
  locales::wire();
  minimap::wire();
  mouse::wire();
  object::wire();
  overlay::wire();
  particle::wire();
  platform::wire();
  runtime::wire();
  sound::wire();
  timer::wire();
  user::wire();

  assert(lua_gettop(L) == 0 && "Lua stack must be empty after API wiring");

  lua_gc(L, LUA_GCSTOP, 0);

  engine e;
  e.run();
}
