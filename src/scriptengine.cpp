namespace {
int protect(lua_State* state, lua_CFunction function) {
  try {
    return function(state);
  } catch (const char* message) {
    lua_pushstring(state, message);
  } catch (const std::exception& exception) {
    lua_pushstring(state, exception.what());
  }

  return lua_error(state);
}
}

static int loader(lua_State *state) {
  std::size_t length;
  const auto* data = luaL_checklstring(state, 1, &length);
  const auto chunk = std::format("@scripts/{}.lua", std::string_view{data, length});
  const auto filename = std::string_view{chunk}.substr(1);
  const auto source = io::read(filename);

  if (luaL_loadbuffer(state, reinterpret_cast<const char *>(source.data()), source.size(), chunk.c_str()) != LUA_OK) [[unlikely]]
    return lua_error(state);

  return 1;
}

void scriptengine::run() {
  lua_gc(L, LUA_GCSTOP, 0);

  lua_pushlightuserdata(L, reinterpret_cast<void *>(protect));
  luaJIT_setmode(L, -1, LUAJIT_MODE_WRAPCFUNC | LUAJIT_MODE_ON);
  lua_pop(L, 1);

  lua_getglobal(L, "package");
  lua_getfield(L, -1, "loaders");

  const auto length = static_cast<int>(lua_objlen(L, -1));
  lua_pushcfunction(L, loader);
  lua_rawseti(L, -2, length + 1);

  lua_pop(L, 2);

  achievement::wire();
  cassette::wire();
  mirror::wire();
  font::wire();
  gamepad::wire();
  keyboard::wire();
  locales::wire();
  mouse::wire();
  objects::wire();
  particleemitter::wire();
  runtime::wire();
  sound::wire();
  user::wire();

  engine e;
  e.run();
}
