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

static constexpr std::array<SDL_Scancode, keys.size + 1> scancodes{
  SDL_SCANCODE_A,
  SDL_SCANCODE_B,
  SDL_SCANCODE_C,
  SDL_SCANCODE_D,
  SDL_SCANCODE_E,
  SDL_SCANCODE_F,
  SDL_SCANCODE_G,
  SDL_SCANCODE_H,
  SDL_SCANCODE_I,
  SDL_SCANCODE_J,
  SDL_SCANCODE_K,
  SDL_SCANCODE_L,
  SDL_SCANCODE_M,
  SDL_SCANCODE_N,
  SDL_SCANCODE_O,
  SDL_SCANCODE_P,
  SDL_SCANCODE_Q,
  SDL_SCANCODE_R,
  SDL_SCANCODE_S,
  SDL_SCANCODE_T,
  SDL_SCANCODE_U,
  SDL_SCANCODE_V,
  SDL_SCANCODE_W,
  SDL_SCANCODE_X,
  SDL_SCANCODE_Y,
  SDL_SCANCODE_Z,
  SDL_SCANCODE_0,
  SDL_SCANCODE_1,
  SDL_SCANCODE_2,
  SDL_SCANCODE_3,
  SDL_SCANCODE_4,
  SDL_SCANCODE_5,
  SDL_SCANCODE_6,
  SDL_SCANCODE_7,
  SDL_SCANCODE_8,
  SDL_SCANCODE_9,
  SDL_SCANCODE_UP,
  SDL_SCANCODE_DOWN,
  SDL_SCANCODE_LEFT,
  SDL_SCANCODE_RIGHT,
  SDL_SCANCODE_LSHIFT,
  SDL_SCANCODE_LCTRL,
  SDL_SCANCODE_ESCAPE,
  SDL_SCANCODE_SPACE,
  SDL_SCANCODE_RETURN,
  SDL_SCANCODE_BACKSPACE,
  SDL_SCANCODE_TAB,
  SDL_SCANCODE_UNKNOWN,
};

static const bool *snapshot;

static int index(lua_State *state) {
  lua_pushboolean(state, snapshot[scancodes[keys.find(lua_tostring(state, 2))]]);

  return 1;
}

void keyboard::wire() {
  keys.intern();
  snapshot = SDL_GetKeyboardState(nullptr);

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
