lua_State *L{nullptr};

SDL_Renderer *renderer{nullptr};

ma_engine audio{};

struct viewport viewport{};

struct depot *depot{nullptr};

const int slot{-3};

const std::uint64_t boot{SDL_GetPerformanceCounter()};
