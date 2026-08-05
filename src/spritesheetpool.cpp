const spritesheet* spritesheetpool::get(std::string_view kind, lua_State* state, int index) {
  const auto key = entt::hashed_string{kind.data(), kind.size()};
  const auto [it, inserted] = _pool.try_emplace(key, nullptr);

  if (inserted) [[unlikely]] {
    lua_pushvalue(state, index);
    const auto table = lua_gettop(state);

    auto entry = std::make_unique<storage>();
    entry->clips.reserve(8);
    entry->frames.reserve(128);
    entry->sheet.pixmap = depot->pixmap.get(std::format("objects/{}", kind));

    const auto iw = 1.f / static_cast<float>(entry->sheet.pixmap->width());
    const auto ih = 1.f / static_cast<float>(entry->sheet.pixmap->height());

    entt::id_type dh = 0;
    lua_getfield(state, table, "default");
    if (lua_isstring(state, -1))
      dh = entt::hashed_string{lua_tostring(state, -1)};
    lua_pop(state, 1);

    uint8_t initial = 0;
    bool collidable = false;

    lua_pushnil(state);
    while (lua_next(state, table)) {
      if (!lua_istable(state, -1)) [[unlikely]] {
        lua_pop(state, 1);
        continue;
      }

      lua_pushvalue(state, -2);
      std::size_t length;
      const auto* data = lua_tolstring(state, -1, &length);
      const auto id = depot->string.get({data, length});
      lua_pop(state, 1);
      const auto name = depot->string.slot(id);

      auto& clip = entry->clips.emplace_back();
      clip.identity.hash = id;
      clip.identity.name_ref = name;
      clip.offset = static_cast<uint16_t>(entry->frames.size());
      clip.count = 0;

      const auto count = static_cast<int>(lua_objlen(state, -1));
      for (int slot = 1; slot <= count; ++slot) {
        lua_rawgeti(state, -1, slot);

        if (!lua_istable(state, -1)) [[unlikely]] {
          lua_pop(state, 1);
          continue;
        }

        auto& frame = entry->frames.emplace_back();

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
        collidable = true;

        ++clip.count;
        lua_pop(state, 1);
      }

      assert(clip.count > 0 && "animation clip must contain at least one frame");
      [[assume(clip.count > 0)]];

      if (id == dh)
        initial = static_cast<uint8_t>(entry->clips.size() - 1);

      lua_getfield(state, -1, "sound");
      if (lua_isstring(state, -1))
        clip.effect = depot->sound.get(std::format("sounds/{}", lua_tostring(state, -1)));
      lua_pop(state, 1);

      lua_pop(state, 1);
    }

    entry->sheet.clips = entry->clips.data();
    entry->sheet.frames = entry->frames.data();
    entry->sheet.count = static_cast<uint8_t>(entry->clips.size());
    entry->sheet.initial = initial;
    entry->sheet.collidable = collidable;

    it->second = std::move(entry);
    lua_pop(state, 1);
  }

  return &it->second->sheet;
}

void spritesheetpool::clear() {
  _pool.clear();
}
