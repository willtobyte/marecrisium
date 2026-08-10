static SDL_Scancode to_scancode(std::string_view key) {
  if (key == "a") return SDL_SCANCODE_A;
  if (key == "b") return SDL_SCANCODE_B;
  if (key == "c") return SDL_SCANCODE_C;
  if (key == "d") return SDL_SCANCODE_D;
  if (key == "e") return SDL_SCANCODE_E;
  if (key == "f") return SDL_SCANCODE_F;
  if (key == "g") return SDL_SCANCODE_G;
  if (key == "h") return SDL_SCANCODE_H;
  if (key == "i") return SDL_SCANCODE_I;
  if (key == "j") return SDL_SCANCODE_J;
  if (key == "k") return SDL_SCANCODE_K;
  if (key == "l") return SDL_SCANCODE_L;
  if (key == "m") return SDL_SCANCODE_M;
  if (key == "n") return SDL_SCANCODE_N;
  if (key == "o") return SDL_SCANCODE_O;
  if (key == "p") return SDL_SCANCODE_P;
  if (key == "q") return SDL_SCANCODE_Q;
  if (key == "r") return SDL_SCANCODE_R;
  if (key == "s") return SDL_SCANCODE_S;
  if (key == "t") return SDL_SCANCODE_T;
  if (key == "u") return SDL_SCANCODE_U;
  if (key == "v") return SDL_SCANCODE_V;
  if (key == "w") return SDL_SCANCODE_W;
  if (key == "x") return SDL_SCANCODE_X;
  if (key == "y") return SDL_SCANCODE_Y;
  if (key == "z") return SDL_SCANCODE_Z;
  if (key == "0") return SDL_SCANCODE_0;
  if (key == "1") return SDL_SCANCODE_1;
  if (key == "2") return SDL_SCANCODE_2;
  if (key == "3") return SDL_SCANCODE_3;
  if (key == "4") return SDL_SCANCODE_4;
  if (key == "5") return SDL_SCANCODE_5;
  if (key == "6") return SDL_SCANCODE_6;
  if (key == "7") return SDL_SCANCODE_7;
  if (key == "8") return SDL_SCANCODE_8;
  if (key == "9") return SDL_SCANCODE_9;
  if (key == "up") return SDL_SCANCODE_UP;
  if (key == "down") return SDL_SCANCODE_DOWN;
  if (key == "left") return SDL_SCANCODE_LEFT;
  if (key == "right") return SDL_SCANCODE_RIGHT;
  if (key == "shift") return SDL_SCANCODE_LSHIFT;
  if (key == "ctrl") return SDL_SCANCODE_LCTRL;
  if (key == "escape") return SDL_SCANCODE_ESCAPE;
  if (key == "space") return SDL_SCANCODE_SPACE;
  if (key == "enter") return SDL_SCANCODE_RETURN;
  if (key == "backspace") return SDL_SCANCODE_BACKSPACE;
  if (key == "tab") return SDL_SCANCODE_TAB;

  return SDL_SCANCODE_UNKNOWN;
}

static int index(lua_State *state) {
  const auto code = to_scancode(luaL_checkstring(state, 2));
  if (code == SDL_SCANCODE_UNKNOWN) {
    lua_pushboolean(state, 0);
    return 1;
  }

  const auto *keyboard = SDL_GetKeyboardState(nullptr);
  lua_pushboolean(state, !!keyboard[code]);
  return 1;
}

void keyboard::wire() {
  luaL_newmetatable(L, "Keyboard");
  lua_pushliteral(L, "Keyboard");
  lua_setfield(L, -2, "__name");

  lua_pushcfunction(L, index);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);

  lua_newuserdata(L, 1);
  luaL_getmetatable(L, "Keyboard");
  lua_setmetatable(L, -2);
  lua_setglobal(L, "keyboard");
}
