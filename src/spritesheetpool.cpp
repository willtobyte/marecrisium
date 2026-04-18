#include "spritesheetpool.hpp"

const spritesheet* spritesheetpool::get(std::string_view kind, lua_State* state, int index) {
  const auto key = entt::hashed_string{kind.data()};
  const auto [it, inserted] = _pool.try_emplace(key, nullptr);

  if (inserted) [[unlikely]] {
    lua_pushvalue(state, index);
    const auto table = lua_gettop(state);

    auto s = std::make_unique<storage>();
    s->clips.reserve(8);
    s->frames.reserve(128);

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
      const std::string_view label = lua_tostring(state, -1);
      lua_pop(state, 1);

      if (label == "default" || label == "sound") [[unlikely]] {
        lua_pop(state, 1);
        continue;
      }

      const auto id = depot->string.get(label);

      auto& c = s->clips.emplace_back();
      c.identity.hash = id;
      c.identity.reference = depot->string.ref(id);
      c.offset = static_cast<uint16_t>(s->frames.size());
      c.count = 0;

      if (id == dh)
        initial = static_cast<uint8_t>(s->clips.size() - 1);

      const auto count = static_cast<int>(lua_objlen(state, -1));
      for (int i = 1; i <= count; ++i) {
        lua_rawgeti(state, -1, i);

        if (!lua_istable(state, -1)) [[unlikely]] {
          lua_pop(state, 1);
          continue;
        }

        auto& fr = s->frames.emplace_back();

        lua_rawgeti(state, -1, 1);
        fr.x = static_cast<float>(lua_tonumber(state, -1));
        lua_pop(state, 1);
        lua_rawgeti(state, -1, 2);
        fr.y = static_cast<float>(lua_tonumber(state, -1));
        lua_pop(state, 1);
        lua_rawgeti(state, -1, 3);
        fr.width = static_cast<float>(lua_tonumber(state, -1));
        lua_pop(state, 1);
        lua_rawgeti(state, -1, 4);
        fr.height = static_cast<float>(lua_tonumber(state, -1));
        lua_pop(state, 1);
        lua_rawgeti(state, -1, 5);
        fr.offset_x = static_cast<float>(lua_tonumber(state, -1));
        lua_pop(state, 1);
        lua_rawgeti(state, -1, 6);
        fr.offset_y = static_cast<float>(lua_tonumber(state, -1));
        lua_pop(state, 1);
        lua_rawgeti(state, -1, 7);
        fr.duration = static_cast<float>(lua_tonumber(state, -1)) / 1000.f;
        lua_pop(state, 1);

        lua_rawgeti(state, -1, 8);
        if (!lua_isnil(state, -1)) {
          fr.bound_x = static_cast<float>(lua_tonumber(state, -1));
          lua_pop(state, 1);
          lua_rawgeti(state, -1, 9);
          fr.bound_y = static_cast<float>(lua_tonumber(state, -1));
          lua_pop(state, 1);
          lua_rawgeti(state, -1, 10);
          fr.bound_width = static_cast<float>(lua_tonumber(state, -1));
          lua_pop(state, 1);
          lua_rawgeti(state, -1, 11);
          fr.bound_height = static_cast<float>(lua_tonumber(state, -1));
          lua_pop(state, 1);
          fr.collidable = true;
          collidable = true;
        } else {
          lua_pop(state, 1);
        }

        ++c.count;
        lua_pop(state, 1);
      }

      lua_getfield(state, -1, "sound");
      if (lua_isstring(state, -1))
        c.effect = depot->sound.get(std::format("sounds/{}", lua_tostring(state, -1)));
      lua_pop(state, 1);

      lua_pop(state, 1);
    }

    s->sheet.pixmap = depot->pixmap.get(std::format("objects/{}", kind));

    const auto iw = 1.f / static_cast<float>(s->sheet.pixmap->width());
    const auto ih = 1.f / static_cast<float>(s->sheet.pixmap->height());
    for (auto& f : s->frames) {
      f.u0 = f.x * iw;
      f.v0 = f.y * ih;
      f.u1 = (f.x + f.width) * iw;
      f.v1 = (f.y + f.height) * ih;
    }

    s->sheet.clips = s->clips.data();
    s->sheet.frames = s->frames.data();
    s->sheet.count = static_cast<uint8_t>(s->clips.size());
    s->sheet.initial = initial;
    s->sheet.collidable = collidable;

    it->second = std::move(s);
    lua_pop(state, 1);
  }

  return &it->second->sheet;
}

void spritesheetpool::clear() {
  _pool.clear();
}
