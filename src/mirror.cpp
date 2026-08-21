void mirror::wire() {
  lua_createtable(L, 0, 4);
  lua_pushinteger(L, static_cast<lua_Integer>(value::none));
  lua_setfield(L, -2, "none");
  lua_pushinteger(L, static_cast<lua_Integer>(value::horizontal));
  lua_setfield(L, -2, "horizontal");
  lua_pushinteger(L, static_cast<lua_Integer>(value::vertical));
  lua_setfield(L, -2, "vertical");
  lua_pushinteger(L, static_cast<lua_Integer>(value::both));
  lua_setfield(L, -2, "both");
  lua_setglobal(L, "mirror");
}
