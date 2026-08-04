static int loader_callback(lua_State *state) {
  const auto filename = std::format("scripts/{}.lua", luaL_checkstring(state, 1));

  const auto buffer = io::read(filename);
  const auto chunk = std::format("@{}", filename);

  if (luaL_loadbuffer(state, reinterpret_cast<const char *>(buffer.data()), buffer.size(), chunk.c_str()) != LUA_OK)
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

  lua_gc(L, LUA_GCSTOP, 0);

  engine e;
  e.run();
}
