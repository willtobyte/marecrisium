namespace error {
[[noreturn]] void raise(lua_State* state) {
  const auto* message = lua_tostring(state, -1);
  auto exception = std::runtime_error{message ? message : "unknown lua error"};
  lua_pop(state, 1);
  throw exception;
}
}
