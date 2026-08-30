lua_State *L{nullptr};

SDL_Renderer *renderer{nullptr};

ma_engine audio{};

struct viewport viewport{};

struct depot *depot{nullptr};

// luaL_ref allocates positive slots; -1 and -2 are its sentinels.
const int slot{-3};

const std::uint64_t boot{SDL_GetPerformanceCounter()};
