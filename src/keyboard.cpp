#include "keyboard.hpp"

static int keyboard_index(lua_State *state) {
  static const ankerl::unordered_dense::map<std::string_view, SDL_Scancode> mapping{
    {"a", SDL_SCANCODE_A}, {"b", SDL_SCANCODE_B}, {"c", SDL_SCANCODE_C},
    {"d", SDL_SCANCODE_D}, {"e", SDL_SCANCODE_E}, {"f", SDL_SCANCODE_F},
    {"g", SDL_SCANCODE_G}, {"h", SDL_SCANCODE_H}, {"i", SDL_SCANCODE_I},
    {"j", SDL_SCANCODE_J}, {"k", SDL_SCANCODE_K}, {"l", SDL_SCANCODE_L},
    {"m", SDL_SCANCODE_M}, {"n", SDL_SCANCODE_N}, {"o", SDL_SCANCODE_O},
    {"p", SDL_SCANCODE_P}, {"q", SDL_SCANCODE_Q}, {"r", SDL_SCANCODE_R},
    {"s", SDL_SCANCODE_S}, {"t", SDL_SCANCODE_T}, {"u", SDL_SCANCODE_U},
    {"v", SDL_SCANCODE_V}, {"w", SDL_SCANCODE_W}, {"x", SDL_SCANCODE_X},
    {"y", SDL_SCANCODE_Y}, {"z", SDL_SCANCODE_Z},
    {"0", SDL_SCANCODE_0}, {"1", SDL_SCANCODE_1}, {"2", SDL_SCANCODE_2},
    {"3", SDL_SCANCODE_3}, {"4", SDL_SCANCODE_4}, {"5", SDL_SCANCODE_5},
    {"6", SDL_SCANCODE_6}, {"7", SDL_SCANCODE_7}, {"8", SDL_SCANCODE_8},
    {"9", SDL_SCANCODE_9},
    {"up", SDL_SCANCODE_UP}, {"down", SDL_SCANCODE_DOWN},
    {"left", SDL_SCANCODE_LEFT}, {"right", SDL_SCANCODE_RIGHT},
    {"shift", SDL_SCANCODE_LSHIFT}, {"ctrl", SDL_SCANCODE_LCTRL},
    {"escape", SDL_SCANCODE_ESCAPE}, {"space", SDL_SCANCODE_SPACE},
    {"enter", SDL_SCANCODE_RETURN}, {"backspace", SDL_SCANCODE_BACKSPACE},
    {"tab", SDL_SCANCODE_TAB},
  };

  const auto key = std::string_view{luaL_checkstring(state, 2)};
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
