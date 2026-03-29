#include "keyboard.hpp"

static int keyboard_index(lua_State *state) {
  static const ankerl::unordered_dense::map<entt::id_type, SDL_Scancode> mapping{
    {"a"_hs.value(), SDL_SCANCODE_A}, {"b"_hs.value(), SDL_SCANCODE_B}, {"c"_hs.value(), SDL_SCANCODE_C},
    {"d"_hs.value(), SDL_SCANCODE_D}, {"e"_hs.value(), SDL_SCANCODE_E}, {"f"_hs.value(), SDL_SCANCODE_F},
    {"g"_hs.value(), SDL_SCANCODE_G}, {"h"_hs.value(), SDL_SCANCODE_H}, {"i"_hs.value(), SDL_SCANCODE_I},
    {"j"_hs.value(), SDL_SCANCODE_J}, {"k"_hs.value(), SDL_SCANCODE_K}, {"l"_hs.value(), SDL_SCANCODE_L},
    {"m"_hs.value(), SDL_SCANCODE_M}, {"n"_hs.value(), SDL_SCANCODE_N}, {"o"_hs.value(), SDL_SCANCODE_O},
    {"p"_hs.value(), SDL_SCANCODE_P}, {"q"_hs.value(), SDL_SCANCODE_Q}, {"r"_hs.value(), SDL_SCANCODE_R},
    {"s"_hs.value(), SDL_SCANCODE_S}, {"t"_hs.value(), SDL_SCANCODE_T}, {"u"_hs.value(), SDL_SCANCODE_U},
    {"v"_hs.value(), SDL_SCANCODE_V}, {"w"_hs.value(), SDL_SCANCODE_W}, {"x"_hs.value(), SDL_SCANCODE_X},
    {"y"_hs.value(), SDL_SCANCODE_Y}, {"z"_hs.value(), SDL_SCANCODE_Z},
    {"0"_hs.value(), SDL_SCANCODE_0}, {"1"_hs.value(), SDL_SCANCODE_1}, {"2"_hs.value(), SDL_SCANCODE_2},
    {"3"_hs.value(), SDL_SCANCODE_3}, {"4"_hs.value(), SDL_SCANCODE_4}, {"5"_hs.value(), SDL_SCANCODE_5},
    {"6"_hs.value(), SDL_SCANCODE_6}, {"7"_hs.value(), SDL_SCANCODE_7}, {"8"_hs.value(), SDL_SCANCODE_8},
    {"9"_hs.value(), SDL_SCANCODE_9},
    {"up"_hs.value(), SDL_SCANCODE_UP}, {"down"_hs.value(), SDL_SCANCODE_DOWN},
    {"left"_hs.value(), SDL_SCANCODE_LEFT}, {"right"_hs.value(), SDL_SCANCODE_RIGHT},
    {"shift"_hs.value(), SDL_SCANCODE_LSHIFT}, {"ctrl"_hs.value(), SDL_SCANCODE_LCTRL},
    {"escape"_hs.value(), SDL_SCANCODE_ESCAPE}, {"space"_hs.value(), SDL_SCANCODE_SPACE},
    {"enter"_hs.value(), SDL_SCANCODE_RETURN}, {"backspace"_hs.value(), SDL_SCANCODE_BACKSPACE},
    {"tab"_hs.value(), SDL_SCANCODE_TAB},
  };

  const auto key = entt::hashed_string{luaL_checkstring(state, 2)}.value();
  const auto it = mapping.find(key);
  if (it == mapping.end()) [[unlikely]]
    return lua_pushnil(state), 1;

  const auto *keyboard = SDL_GetKeyboardState(nullptr);
  lua_pushboolean(state, keyboard[it->second] != 0 ? 1 : 0);
  return 1;
}

void keyboard::wire() {
  metatable(L, "Keyboard", keyboard_index);

  singleton(L, "Keyboard", "keyboard");
}
