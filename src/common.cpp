#include "common.hpp"

lua_State *L = nullptr;
SDL_Renderer *renderer = nullptr;
ma_engine *audioengine = nullptr;
struct viewport viewport{};
struct resources *depot = nullptr;
