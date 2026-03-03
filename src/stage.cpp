#include "stage.hpp"

stage::stage(std::string_view name)
    : _name(name),
      _pixmaps(std::make_unique<pixmappool>()),
      _sources(std::make_unique<sourcepool>()),
      _strings(std::make_unique<stringpool>()) {
  b2WorldDef def = b2DefaultWorldDef();
  def.gravity = {.0f, .0f};
  _world = b2CreateWorld(&def);

  _registry.on_destroy<objectproxy>().connect<&objectproxy::on_destroy>();

  _registry.ctx().emplace<sourcepool*>(_sources.get());
  _registry.ctx().emplace<stringpool*>(_strings.get());

  lua_newtable(L);
  lua_newtable(L);
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  lua_setfield(L, -2, "__index");
  lua_setmetatable(L, -2);
  _environment_reference = luaL_ref(L, LUA_REGISTRYINDEX);

  const auto filename = std::format("stages/{}.lua", name);
  const auto buffer = io::read(filename);
  const auto *data = reinterpret_cast<const char *>(buffer.data());
  const auto size = buffer.size();
  const auto label = std::format("@{}", filename);

  if (luaL_loadbuffer(L, data, size, label.c_str()) != 0) [[unlikely]] {
    std::string error = lua_tostring(L, -1);
    lua_pop(L, 1);
    throw std::runtime_error(error);
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, _environment_reference);
  lua_setfenv(L, -2);

  if (lua_pcall(L, 0, 1, 0) != 0) [[unlikely]] {
    std::string error = lua_tostring(L, -1);
    lua_pop(L, 1);
    throw std::runtime_error(error);
  }

  lua_getfield(L, -1, "objects");
  if (lua_istable(L, -1)) {
    const auto count = static_cast<int>(lua_objlen(L, -1));

    lua_newtable(L);

    for (int i = 1; i <= count; ++i) {
      lua_rawgeti(L, -2, i);
      const std::string_view object_name = lua_tostring(L, -1);
      lua_pop(L, 1);

      _pixmaps->load(_name, object_name);
      _sources->load(_name, object_name);

      const auto entity = _registry.create();
      _registry.emplace<transform>(entity);
      const auto& proxy = _registry.emplace<objectproxy>(entity, _registry, entity, _name, object_name, _environment_reference);
      const auto object_reference = proxy.object_reference;
      const auto self_reference = proxy.self_reference;

      const auto kind_hash = entt::hashed_string{object_name.data()}.value();

      lua_rawgeti(L, LUA_REGISTRYINDEX, object_reference);
      lua_getfield(L, -1, "animation");

      if (lua_istable(L, -1)) {
        auto& a = _registry.emplace<animation>(entity);
        a.pixmap = &_pixmaps->get(kind_hash);

        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
          if (a.clip_count >= a.clips.size()) {
            lua_pop(L, 2);
            break;
          }

          const std::string_view clip_name = lua_tostring(L, -2);
          const auto clip_hash = entt::hashed_string{clip_name.data()}.value();

          _strings->insert(clip_hash, clip_name);

          auto& c = a.clips[a.clip_count];
          c.name = clip_hash;
          c.count = 0;

          if (lua_istable(L, -1)) {
            const auto frame_count = static_cast<int>(lua_objlen(L, -1));

            for (int f = 1; f <= frame_count && c.count < c.frames.size(); ++f) {
              lua_rawgeti(L, -1, f);

              if (lua_istable(L, -1)) {
                auto& fr = c.frames[c.count];

                lua_rawgeti(L, -1, 1);
                fr.x = static_cast<float>(lua_tonumber(L, -1));
                lua_pop(L, 1);

                lua_rawgeti(L, -1, 2);
                fr.y = static_cast<float>(lua_tonumber(L, -1));
                lua_pop(L, 1);

                lua_rawgeti(L, -1, 3);
                fr.w = static_cast<float>(lua_tonumber(L, -1));
                lua_pop(L, 1);

                lua_rawgeti(L, -1, 4);
                fr.h = static_cast<float>(lua_tonumber(L, -1));
                lua_pop(L, 1);

                lua_rawgeti(L, -1, 5);
                fr.duration = static_cast<float>(lua_tonumber(L, -1)) / 1000.0f;
                lua_pop(L, 1);

                ++c.count;
              }

              lua_pop(L, 1);
            }
          }

          ++a.clip_count;
          lua_pop(L, 1);
        }
      }

      lua_pop(L, 2);

      lua_rawgeti(L, LUA_REGISTRYINDEX, object_reference);
      lua_getfield(L, -1, "on_spawn");

      if (lua_isfunction(L, -1)) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, self_reference);

        if (lua_pcall(L, 1, 0, 0) != 0) [[unlikely]] {
          std::string error = lua_tostring(L, -1);
          lua_pop(L, 1);
          throw std::runtime_error(error);
        }
      } else {
        lua_pop(L, 1);
      }

      lua_pop(L, 1);

      lua_rawgeti(L, LUA_REGISTRYINDEX, self_reference);
      lua_setfield(L, -2, object_name.data());
    }

    _pool_reference = luaL_ref(L, LUA_REGISTRYINDEX);

    lua_rawgeti(L, LUA_REGISTRYINDEX, _environment_reference);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_reference);
    lua_setfield(L, -2, "pool");
    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  _reference = luaL_ref(L, LUA_REGISTRYINDEX);
}

stage::~stage() {
  luaL_unref(L, LUA_REGISTRYINDEX, _pool_reference);
  luaL_unref(L, LUA_REGISTRYINDEX, _environment_reference);
  luaL_unref(L, LUA_REGISTRYINDEX, _reference);
  b2DestroyWorld(_world);
}

void stage::on_enter() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
  lua_getfield(L, -1, "on_enter");

  if (lua_isfunction(L, -1)) {
    if (lua_pcall(L, 0, 0, 0) != 0) [[unlikely]] {
      std::string error = lua_tostring(L, -1);
      lua_pop(L, 2);
      throw std::runtime_error(error);
    }
  } else {
    lua_pop(L, 1);
  }

  lua_pop(L, 1);
}

void stage::on_leave() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
  lua_getfield(L, -1, "on_leave");

  if (lua_isfunction(L, -1)) {
    if (lua_pcall(L, 0, 0, 0) != 0) [[unlikely]] {
      std::string error = lua_tostring(L, -1);
      lua_pop(L, 2);
      throw std::runtime_error(error);
    }
  } else {
    lua_pop(L, 1);
  }

  lua_pop(L, 1);
}

void stage::update(float delta) {
  _accumulator += delta;

  while (_accumulator >= FIXED_TIMESTEP) {
    b2World_Step(_world, FIXED_TIMESTEP, WORLD_SUBSTEPS);
    _accumulator -= FIXED_TIMESTEP;
  }

  for (auto&& [entity, a] : _registry.view<animation>().each()) {
    if (!a.playing || a.clip_count == 0)
      continue;

    const auto& c = a.clips[a.active];
    if (c.count == 0)
      continue;

    const auto& fr = c.frames[a.current];

    if (fr.duration < 0.0f)
      continue;

    a.elapsed += delta;

    while (a.elapsed >= fr.duration && fr.duration > 0.0f) {
      a.elapsed -= fr.duration;
      ++a.current;

      if (a.current >= c.count) {
        a.current = 0;

        if (_registry.all_of<objectproxy>(entity)) {
          const auto& proxy = _registry.get<objectproxy>(entity);

          if (proxy.object_reference != LUA_NOREF && proxy.self_reference != LUA_NOREF) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.object_reference);
            lua_getfield(L, -1, "on_animation_end");

            if (lua_isfunction(L, -1)) {
              lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.self_reference);
              lua_pushstring(L, _strings->get(c.name));

              if (lua_pcall(L, 2, 0, 0) != 0) [[unlikely]] {
                std::string error = lua_tostring(L, -1);
                lua_pop(L, 1);
                throw std::runtime_error(error);
              }
            } else {
              lua_pop(L, 1);
            }

            lua_pop(L, 1);

            lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.object_reference);
            lua_getfield(L, -1, "on_animation_begin");

            if (lua_isfunction(L, -1)) {
              lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.self_reference);
              lua_pushstring(L, _strings->get(c.name));

              if (lua_pcall(L, 2, 0, 0) != 0) [[unlikely]] {
                std::string error = lua_tostring(L, -1);
                lua_pop(L, 1);
                throw std::runtime_error(error);
              }
            } else {
              lua_pop(L, 1);
            }

            lua_pop(L, 1);
          }
        }
      }

      break;
    }
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
  lua_getfield(L, -1, "on_loop");

  if (lua_isfunction(L, -1)) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
    lua_pushnumber(L, static_cast<lua_Number>(delta));

    if (lua_pcall(L, 2, 0, 0) != 0) [[unlikely]] {
      std::string error = lua_tostring(L, -1);
      lua_pop(L, 2);
      throw std::runtime_error(error);
    }
  } else {
    lua_pop(L, 1);
  }

  lua_pop(L, 1);
}

void stage::draw() const {
  for (auto&& [entity, a, tf] : _registry.view<animation, transform>().each()) {
    if (!tf.shown || !a.playing || !a.pixmap || a.clip_count == 0) [[unlikely]]
      continue;

    const auto& c = a.clips[a.active];
    if (c.count == 0)
      continue;

    const auto& fr = c.frames[a.current];

    const auto dw = fr.w * tf.scale;
    const auto dh = fr.h * tf.scale;

    a.pixmap->draw(
      fr.x, fr.y, fr.w, fr.h,
      tf.x, tf.y, dw, dh,
      static_cast<double>(tf.angle),
      tf.alpha
    );
  }

#ifdef DEVELOPMENT
  SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);

  const b2AABB aabb = {{0, 0}, {viewport.width, viewport.height}};
  const b2QueryFilter filter = b2DefaultQueryFilter();

  b2World_OverlapAABB(_world, aabb, filter, [](b2ShapeId shape, void*) -> bool {
    const auto box = b2Shape_GetAABB(shape);
    const SDL_FRect r{
      box.lowerBound.x,
      box.lowerBound.y,
      box.upperBound.x - box.lowerBound.x,
      box.upperBound.y - box.lowerBound.y
    };

    SDL_RenderRect(renderer, &r);

    return true;
  }, nullptr);

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
#endif
}
