const spritesheet* spritesheetpool::get(std::string_view kind, lua_State* state, int index) {
  if (const auto it = _pool.find(kind); it != _pool.end()) [[likely]]
    return &it->second->sheet;

  lua_pushvalue(state, index);
  const auto top = lua_gettop(state);

  auto storage = std::make_unique<class storage>();
  storage->clips.reserve(8);
  storage->frames.reserve(128);
  storage->sheet.pixmap = depot->get<pixmap>(std::format("objects/{}", kind));

  const auto iw = 1.f / static_cast<float>(storage->sheet.pixmap->width());
  const auto ih = 1.f / static_cast<float>(storage->sheet.pixmap->height());

  std::string_view primary;
  lua_getfield(state, top, "default");
  if (lua_isstring(state, -1)) {
    std::size_t length;
    const auto* data = lua_tolstring(state, -1, &length);
    primary = {data, length};
  }
  lua_pop(state, 1);

  uint8_t initial = 0;

  lua_pushnil(state);
  while (lua_next(state, top)) {
    if (!lua_istable(state, -1)) [[unlikely]] {
      lua_pop(state, 1);
      continue;
    }

    lua_pushvalue(state, -2);
    std::size_t length;
    const auto* data = lua_tolstring(state, -1, &length);
    const std::string_view name{data, length};
    const auto slot = luaL_ref(state, LUA_REGISTRYINDEX);

    auto& clip = storage->clips.emplace_back();
    clip.name = slot;
    clip.offset = static_cast<uint16_t>(storage->frames.size());
    clip.count = 0;

    const auto count = static_cast<int>(lua_objlen(state, -1));
    for (int slot = 1; slot <= count; ++slot) {
      lua_rawgeti(state, -1, slot);

      const auto istable = lua_istable(state, -1);
      assert(istable && "animation frame must be a table");
      [[assume(istable)]];

      auto& frame = storage->frames.emplace_back();

      lua_rawgeti(state, -1, 1);
      const auto x = static_cast<float>(lua_tonumber(state, -1));
      lua_pop(state, 1);
      lua_rawgeti(state, -1, 2);
      const auto y = static_cast<float>(lua_tonumber(state, -1));
      lua_pop(state, 1);
      lua_rawgeti(state, -1, 3);
      frame.width = static_cast<float>(lua_tonumber(state, -1));
      lua_pop(state, 1);
      lua_rawgeti(state, -1, 4);
      frame.height = static_cast<float>(lua_tonumber(state, -1));
      lua_pop(state, 1);
      lua_rawgeti(state, -1, 5);
      frame.duration = static_cast<float>(lua_tonumber(state, -1)) / 1000.f;
      lua_pop(state, 1);

      frame.u0 = x * iw;
      frame.v0 = y * ih;
      frame.u1 = (x + frame.width) * iw;
      frame.v1 = (y + frame.height) * ih;

      lua_rawgeti(state, -1, 6);
      if (lua_isnil(state, -1)) {
        lua_pop(state, 1);
        ++clip.count;
        lua_pop(state, 1);
        continue;
      }

      frame.collider.offset_x = static_cast<float>(lua_tonumber(state, -1));
      lua_pop(state, 1);
      lua_rawgeti(state, -1, 7);
      frame.collider.offset_y = static_cast<float>(lua_tonumber(state, -1));
      lua_pop(state, 1);
      lua_rawgeti(state, -1, 8);
      frame.collider.width = static_cast<float>(lua_tonumber(state, -1));
      lua_pop(state, 1);
      lua_rawgeti(state, -1, 9);
      frame.collider.height = static_cast<float>(lua_tonumber(state, -1));
      lua_pop(state, 1);

      ++clip.count;
      lua_pop(state, 1);
    }

    assert(clip.count > 0 && "animation clip must contain at least one frame");
    [[assume(clip.count > 0)]];

    if (name == primary)
      initial = static_cast<uint8_t>(storage->clips.size() - 1);

    lua_pop(state, 1);
  }

  storage->sheet.clips = storage->clips.data();
  storage->sheet.frames = storage->frames.data();
  storage->sheet.count = static_cast<uint8_t>(storage->clips.size());
  storage->sheet.initial = initial;

  auto* result = &storage->sheet;
  _pool.emplace(kind, std::move(storage));
  lua_pop(state, 1);
  return result;
}

spritesheetpool::~spritesheetpool() {
  clear();
}

void spritesheetpool::clear() {
  for (const auto& [_, storage] : _pool)
    for (const auto& clip : storage->clips)
      luaL_unref(L, LUA_REGISTRYINDEX, clip.name);

  _pool.clear();
}
