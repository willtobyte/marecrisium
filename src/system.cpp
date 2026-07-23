static int runtime_moment(lua_State *state) {
  lua_pushnumber(state, static_cast<lua_Number>(SDL_GetTicks()));
  return 1;
}

void runtime::wire() {
  binding::callback(L, runtime_moment);
  lua_setglobal(L, "moment");
}
