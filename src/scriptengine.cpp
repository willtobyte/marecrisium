#include "scriptengine.hpp"

static int searcher(lua_State *state) {
  const auto *module = luaL_checkstring(state, 1);
  const auto filename = std::format("scripts/{}.lua", module);
  const auto buffer = io::read(filename);
  const auto *data = reinterpret_cast<const char *>(buffer.data());
  const auto size = buffer.size();
  const auto label = std::format("@{}", filename);

  luaL_loadbuffer(state, data, size, label.c_str());

  return 1;
}

void scriptengine::run() {
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "loaders");

  const auto lenght = static_cast<int>(lua_objlen(L, -1));
  lua_pushcfunction(L, searcher);
  lua_rawseti(L, -2, lenght + 1);

  lua_pop(L, 2);

  gamepad::wire();

  auto e = engine();
  e.run();
}
