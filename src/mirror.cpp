void mirror::wire() {
  lua_createtable(L, 0, 4);
  lua_pushinteger(L, std::to_underlying(value::none));
  lua_setfield(L, -2, "none");
  lua_pushinteger(L, std::to_underlying(value::horizontal));
  lua_setfield(L, -2, "horizontal");
  lua_pushinteger(L, std::to_underlying(value::vertical));
  lua_setfield(L, -2, "vertical");
  lua_pushinteger(L, std::to_underlying(value::both));
  lua_setfield(L, -2, "both");
  lua_setglobal(L, "mirror");
}
