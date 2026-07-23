static int loader(lua_State *state) {
  const std::string_view module = luaL_checkstring(state, 1);
  const auto filename = std::format("scripts/{}.lua", module);

  const auto buffer = io::read(filename);
  const auto label = std::format("@{}", filename);

  if (luaL_loadbuffer(state, reinterpret_cast<const char *>(buffer.data()), buffer.size(), label.c_str()) != 0)
    return lua_error(state);

  return 1;
}

void scriptengine::run() {
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "loaders");

  const auto length = static_cast<int>(lua_objlen(L, -1));
  binding::callback(L, loader);
  lua_rawseti(L, -2, length + 1);

  lua_pop(L, 2);

  achievement::wire();
  cassette::wire();
  font::wire();
  foreground::wire();
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
  user::wire();
  xorshift128::wire();

  lua_gc(L, LUA_GCSTOP, 0);

  engine e;
  e.run();
}
