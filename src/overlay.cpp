overlay::overlay(std::string_view name) {
  const auto chunk = std::format("@overlays/{}.lua", name);
  const auto path = std::string_view{chunk}.substr(1);
  const auto source = io::read(path);
  error::check(L, luaL_loadbuffer(L, reinterpret_cast<const char*>(source.data()), source.size(), chunk.c_str()));

  const auto top = lua_gettop(L);
  lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
  lua_insert(L, top);
  const auto status = lua_pcall(L, 0, 1, top);
  lua_remove(L, top);
  error::check(L, status);

  lua_getfield(L, top, "fonts");
  const auto fonts = static_cast<int>(lua_objlen(L, -1));

  for (auto i = 1; i <= fonts; ++i) {
    lua_rawgeti(L, -1, i);
    const auto *family = luaL_checkstring(L, -1);
    auto **memory = static_cast<font **>(lua_newuserdata(L, sizeof(font *)));
    *memory = depot->font.get(family);
    luaL_getmetatable(L, "Font");
    lua_setmetatable(L, -2);
    lua_setfield(L, top, family);
    lua_pop(L, 1);
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
}

overlay::~overlay() {
  luaL_unref(L, LUA_REGISTRYINDEX, _on_paint);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_loop);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_disappear);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_appear);
  luaL_unref(L, LUA_REGISTRYINDEX, _table);
}

void overlay::appear() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
  const auto top = lua_gettop(L);
  lua_getfield(L, top, "fonts");
  const auto fonts = static_cast<int>(lua_objlen(L, top + 1));
  lua_getglobal(L, "pool");

  for (auto i = 1; i <= fonts; ++i) {
    lua_rawgeti(L, top + 1, i);
    const auto *family = luaL_checkstring(L, -1);
    lua_getfield(L, top, family);
    lua_setfield(L, top + 2, family);
    lua_pop(L, 1);
  }
  lua_pop(L, 3);

  if (_on_appear != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_appear);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
    const auto top = lua_gettop(L) - 1;
    lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
    lua_insert(L, top);
    const auto status = lua_pcall(L, 1, 0, top);
    lua_remove(L, top);
    error::check(L, status);
  }
}

void overlay::disappear() {
  if (_on_disappear != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_disappear);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
    const auto top = lua_gettop(L) - 1;
    lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
    lua_insert(L, top);
    const auto status = lua_pcall(L, 1, 0, top);
    lua_remove(L, top);
    error::check(L, status);
  }
}

void overlay::update(float delta) {
  if (_on_loop != LUA_NOREF) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_loop);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
    lua_pushnumber(L, static_cast<lua_Number>(delta));
    const auto top = lua_gettop(L) - 2;
    lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
    lua_insert(L, top);
    const auto status = lua_pcall(L, 2, 0, top);
    lua_remove(L, top);
    error::check(L, status);
  }
}

void overlay::draw() {
  if (_on_paint != LUA_NOREF) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_paint);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
    const auto top = lua_gettop(L) - 1;
    lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
    lua_insert(L, top);
    const auto status = lua_pcall(L, 1, 0, top);
    lua_remove(L, top);
    error::check(L, status);
  }
}
