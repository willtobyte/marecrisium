static keyring<
  "a",
  "b",
  "c",
  "d",
  "e",
  "f",
  "g",
  "h",
  "i",
  "j",
  "k",
  "l",
  "m",
  "n",
  "o",
  "p",
  "q",
  "r",
  "s",
  "t",
  "u",
  "v",
  "w",
  "x",
  "y",
  "z",
  "0",
  "1",
  "2",
  "3",
  "4",
  "5",
  "6",
  "7",
  "8",
  "9",
  "up",
  "down",
  "left",
  "right",
  "shift",
  "ctrl",
  "escape",
  "space",
  "enter",
  "backspace",
  "tab"
> keys;

static SDL_Scancode to_scancode(const char *key) {
  switch (keys.find(key)) {
  case keys.id<"a">(): return SDL_SCANCODE_A;
  case keys.id<"b">(): return SDL_SCANCODE_B;
  case keys.id<"c">(): return SDL_SCANCODE_C;
  case keys.id<"d">(): return SDL_SCANCODE_D;
  case keys.id<"e">(): return SDL_SCANCODE_E;
  case keys.id<"f">(): return SDL_SCANCODE_F;
  case keys.id<"g">(): return SDL_SCANCODE_G;
  case keys.id<"h">(): return SDL_SCANCODE_H;
  case keys.id<"i">(): return SDL_SCANCODE_I;
  case keys.id<"j">(): return SDL_SCANCODE_J;
  case keys.id<"k">(): return SDL_SCANCODE_K;
  case keys.id<"l">(): return SDL_SCANCODE_L;
  case keys.id<"m">(): return SDL_SCANCODE_M;
  case keys.id<"n">(): return SDL_SCANCODE_N;
  case keys.id<"o">(): return SDL_SCANCODE_O;
  case keys.id<"p">(): return SDL_SCANCODE_P;
  case keys.id<"q">(): return SDL_SCANCODE_Q;
  case keys.id<"r">(): return SDL_SCANCODE_R;
  case keys.id<"s">(): return SDL_SCANCODE_S;
  case keys.id<"t">(): return SDL_SCANCODE_T;
  case keys.id<"u">(): return SDL_SCANCODE_U;
  case keys.id<"v">(): return SDL_SCANCODE_V;
  case keys.id<"w">(): return SDL_SCANCODE_W;
  case keys.id<"x">(): return SDL_SCANCODE_X;
  case keys.id<"y">(): return SDL_SCANCODE_Y;
  case keys.id<"z">(): return SDL_SCANCODE_Z;
  case keys.id<"0">(): return SDL_SCANCODE_0;
  case keys.id<"1">(): return SDL_SCANCODE_1;
  case keys.id<"2">(): return SDL_SCANCODE_2;
  case keys.id<"3">(): return SDL_SCANCODE_3;
  case keys.id<"4">(): return SDL_SCANCODE_4;
  case keys.id<"5">(): return SDL_SCANCODE_5;
  case keys.id<"6">(): return SDL_SCANCODE_6;
  case keys.id<"7">(): return SDL_SCANCODE_7;
  case keys.id<"8">(): return SDL_SCANCODE_8;
  case keys.id<"9">(): return SDL_SCANCODE_9;
  case keys.id<"up">(): return SDL_SCANCODE_UP;
  case keys.id<"down">(): return SDL_SCANCODE_DOWN;
  case keys.id<"left">(): return SDL_SCANCODE_LEFT;
  case keys.id<"right">(): return SDL_SCANCODE_RIGHT;
  case keys.id<"shift">(): return SDL_SCANCODE_LSHIFT;
  case keys.id<"ctrl">(): return SDL_SCANCODE_LCTRL;
  case keys.id<"escape">(): return SDL_SCANCODE_ESCAPE;
  case keys.id<"space">(): return SDL_SCANCODE_SPACE;
  case keys.id<"enter">(): return SDL_SCANCODE_RETURN;
  case keys.id<"backspace">(): return SDL_SCANCODE_BACKSPACE;
  case keys.id<"tab">(): return SDL_SCANCODE_TAB;

  default: return SDL_SCANCODE_UNKNOWN;
  }
}

static int index(lua_State *state) {
  const auto code = to_scancode(lua_tostring(state, 2));
  if (code == SDL_SCANCODE_UNKNOWN) {
    lua_pushboolean(state, 0);

    return 1;
  }

  const auto *keyboard = SDL_GetKeyboardState(nullptr);
  lua_pushboolean(state, !!keyboard[code]);

  return 1;
}

void keyboard::wire() {
  keys.intern();

  lua_createtable(L, 0, 2);
  lua_pushliteral(L, "Keyboard");
  lua_setfield(L, -2, "__name");

  lua_pushcfunction(L, index);
  lua_setfield(L, -2, "__index");

  lua_newuserdata(L, 1);
  lua_pushvalue(L, -2);
  lua_setmetatable(L, -2);
  lua_setglobal(L, "keyboard");
  lua_setfield(L, LUA_REGISTRYINDEX, "Keyboard");
}
