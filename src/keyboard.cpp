#include "keyboard.hpp"

static int keyboard_index(lua_State *state) {
  static const ankerl::unordered_dense::map<entt::id_type, SDL_Scancode> mapping{
    {"a"_hs, SDL_SCANCODE_A}, {"b"_hs, SDL_SCANCODE_B}, {"c"_hs, SDL_SCANCODE_C},
    {"d"_hs, SDL_SCANCODE_D}, {"e"_hs, SDL_SCANCODE_E}, {"f"_hs, SDL_SCANCODE_F},
    {"g"_hs, SDL_SCANCODE_G}, {"h"_hs, SDL_SCANCODE_H}, {"i"_hs, SDL_SCANCODE_I},
    {"j"_hs, SDL_SCANCODE_J}, {"k"_hs, SDL_SCANCODE_K}, {"l"_hs, SDL_SCANCODE_L},
    {"m"_hs, SDL_SCANCODE_M}, {"n"_hs, SDL_SCANCODE_N}, {"o"_hs, SDL_SCANCODE_O},
    {"p"_hs, SDL_SCANCODE_P}, {"q"_hs, SDL_SCANCODE_Q}, {"r"_hs, SDL_SCANCODE_R},
    {"s"_hs, SDL_SCANCODE_S}, {"t"_hs, SDL_SCANCODE_T}, {"u"_hs, SDL_SCANCODE_U},
    {"v"_hs, SDL_SCANCODE_V}, {"w"_hs, SDL_SCANCODE_W}, {"x"_hs, SDL_SCANCODE_X},
    {"y"_hs, SDL_SCANCODE_Y}, {"z"_hs, SDL_SCANCODE_Z},
    {"0"_hs, SDL_SCANCODE_0}, {"1"_hs, SDL_SCANCODE_1}, {"2"_hs, SDL_SCANCODE_2},
    {"3"_hs, SDL_SCANCODE_3}, {"4"_hs, SDL_SCANCODE_4}, {"5"_hs, SDL_SCANCODE_5},
    {"6"_hs, SDL_SCANCODE_6}, {"7"_hs, SDL_SCANCODE_7}, {"8"_hs, SDL_SCANCODE_8},
    {"9"_hs, SDL_SCANCODE_9},
    {"up"_hs, SDL_SCANCODE_UP}, {"down"_hs, SDL_SCANCODE_DOWN},
    {"left"_hs, SDL_SCANCODE_LEFT}, {"right"_hs, SDL_SCANCODE_RIGHT},
    {"shift"_hs, SDL_SCANCODE_LSHIFT}, {"ctrl"_hs, SDL_SCANCODE_LCTRL},
    {"escape"_hs, SDL_SCANCODE_ESCAPE}, {"space"_hs, SDL_SCANCODE_SPACE},
    {"enter"_hs, SDL_SCANCODE_RETURN}, {"backspace"_hs, SDL_SCANCODE_BACKSPACE},
    {"tab"_hs, SDL_SCANCODE_TAB},
  };

  const auto id = entt::hashed_string{luaL_checkstring(state, 2)};
  const auto it = mapping.find(id);
  if (it == mapping.end()) [[unlikely]]
    return lua_pushnil(state), 1;

  const auto *keyboard = SDL_GetKeyboardState(nullptr);
  lua_pushboolean(state, !!keyboard[it->second]);
  return 1;
}

void keyboard::wire() {
  metatable(L, "Keyboard", keyboard_index);

  singleton(L, "Keyboard", "keyboard");
}
