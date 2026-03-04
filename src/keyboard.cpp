#include "keyboard.hpp"

static int keyboard_index(lua_State *state) {
  const std::string_view key = luaL_checkstring(state, 2);
  SDL_Scancode code;

  if (key == "a") code = SDL_SCANCODE_A;
  else if (key == "b") code = SDL_SCANCODE_B;
  else if (key == "c") code = SDL_SCANCODE_C;
  else if (key == "d") code = SDL_SCANCODE_D;
  else if (key == "e") code = SDL_SCANCODE_E;
  else if (key == "f") code = SDL_SCANCODE_F;
  else if (key == "g") code = SDL_SCANCODE_G;
  else if (key == "h") code = SDL_SCANCODE_H;
  else if (key == "i") code = SDL_SCANCODE_I;
  else if (key == "j") code = SDL_SCANCODE_J;
  else if (key == "k") code = SDL_SCANCODE_K;
  else if (key == "l") code = SDL_SCANCODE_L;
  else if (key == "m") code = SDL_SCANCODE_M;
  else if (key == "n") code = SDL_SCANCODE_N;
  else if (key == "o") code = SDL_SCANCODE_O;
  else if (key == "p") code = SDL_SCANCODE_P;
  else if (key == "q") code = SDL_SCANCODE_Q;
  else if (key == "r") code = SDL_SCANCODE_R;
  else if (key == "s") code = SDL_SCANCODE_S;
  else if (key == "t") code = SDL_SCANCODE_T;
  else if (key == "u") code = SDL_SCANCODE_U;
  else if (key == "v") code = SDL_SCANCODE_V;
  else if (key == "w") code = SDL_SCANCODE_W;
  else if (key == "x") code = SDL_SCANCODE_X;
  else if (key == "y") code = SDL_SCANCODE_Y;
  else if (key == "z") code = SDL_SCANCODE_Z;
  else if (key == "0") code = SDL_SCANCODE_0;
  else if (key == "1") code = SDL_SCANCODE_1;
  else if (key == "2") code = SDL_SCANCODE_2;
  else if (key == "3") code = SDL_SCANCODE_3;
  else if (key == "4") code = SDL_SCANCODE_4;
  else if (key == "5") code = SDL_SCANCODE_5;
  else if (key == "6") code = SDL_SCANCODE_6;
  else if (key == "7") code = SDL_SCANCODE_7;
  else if (key == "8") code = SDL_SCANCODE_8;
  else if (key == "9") code = SDL_SCANCODE_9;
  else if (key == "up") code = SDL_SCANCODE_UP;
  else if (key == "down") code = SDL_SCANCODE_DOWN;
  else if (key == "left") code = SDL_SCANCODE_LEFT;
  else if (key == "right") code = SDL_SCANCODE_RIGHT;
  else if (key == "shift") code = SDL_SCANCODE_LSHIFT;
  else if (key == "ctrl") code = SDL_SCANCODE_LCTRL;
  else if (key == "escape") code = SDL_SCANCODE_ESCAPE;
  else if (key == "space") code = SDL_SCANCODE_SPACE;
  else if (key == "enter") code = SDL_SCANCODE_RETURN;
  else if (key == "backspace") code = SDL_SCANCODE_BACKSPACE;
  else if (key == "tab") code = SDL_SCANCODE_TAB;
  else [[unlikely]]
    return lua_pushnil(state), 1;

  const auto *keyboard = SDL_GetKeyboardState(nullptr);
  lua_pushboolean(state, keyboard[code]);
  return 1;
}

void keyboard::wire() {
  lua_newuserdata(L, 1);

  luaL_newmetatable(L, "Keyboard");
  lua_pushcfunction(L, keyboard_index);
  lua_setfield(L, -2, "__index");

  lua_setmetatable(L, -2);
  lua_setglobal(L, "keyboard");
}
