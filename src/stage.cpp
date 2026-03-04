#include "stage.hpp"

static int world_raycast(lua_State* state) {
  auto* self = static_cast<stage*>(lua_touserdata(state, lua_upvalueindex(1)));
  const auto* caller = static_cast<objectproxy*>(luaL_checkudata(state, 1, "Object"));
  const auto x = static_cast<float>(luaL_checknumber(state, 2));
  const auto y = static_cast<float>(luaL_checknumber(state, 3));
  const auto angle = static_cast<float>(luaL_checknumber(state, 4));
  const auto distance = static_cast<float>(luaL_checknumber(state, 5));
  return self->raycast(state, caller->entity, x, y, angle, distance);
}

stage::stage(std::string_view name)
    : _name(name),
      _pixmaps(std::make_unique<pixmappool>()),
      _sources(std::make_unique<sourcepool>()),
      _strings(std::make_unique<stringpool>()) {
  b2WorldDef def = b2DefaultWorldDef();
  def.gravity = {.0f, .0f};
  _world = b2CreateWorld(&def);

  _registry.on_destroy<objectproxy>().connect<&objectproxy::on_destroy>();
  _registry.on_destroy<body>().connect<[](entt::registry& registry, entt::entity entity) {
    auto& pb = registry.get<body>(entity);
    if (b2Body_IsValid(pb.id))
      b2DestroyBody(pb.id);
  }>();

  _registry.ctx().emplace<sourcepool*>(_sources.get());
  _registry.ctx().emplace<stringpool*>(_strings.get());

  lua_newtable(L);
  lua_newtable(L);
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  lua_setfield(L, -2, "__index");
  lua_setmetatable(L, -2);
  _environment_reference = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _environment_reference);
  lua_newtable(L);
  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, world_raycast, 1);
  lua_setfield(L, -2, "raycast");
  lua_setfield(L, -2, "world");
  lua_pop(L, 1);

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

      lua_getfield(L, -1, "name");
      const std::string_view object_name = lua_tostring(L, -1);
      lua_pop(L, 1);

      lua_getfield(L, -1, "kind");
      const std::string_view object_kind = lua_tostring(L, -1);
      lua_pop(L, 1);

      lua_pop(L, 1);

      const auto entity = _registry.create();
      _registry.emplace<transform>(entity);
      const auto& proxy = _registry.emplace<objectproxy>(entity, _registry, entity, _name, object_name, object_kind, _environment_reference);
      const auto prototype = proxy.prototype;
      const auto handle = proxy.handle;

      _strings->insert(object_name);
      _strings->insert(object_kind);

      lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);
      lua_getfield(L, -1, "animation");

      if (lua_istable(L, -1)) {
        auto& a = _registry.emplace<animation>(entity);
        a.pixmap = &_pixmaps->get(_name, object_kind);

        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
          if (a.clip_count >= a.clips.size()) {
            lua_pop(L, 2);
            break;
          }

          const std::string_view clip_name = lua_tostring(L, -2);
          const auto cid = _strings->insert(clip_name);

          auto& c = a.clips[a.clip_count];
          c.name = cid;
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

                lua_rawgeti(L, -1, 6);
                if (!lua_isnil(L, -1)) {
                  fr.cx = static_cast<float>(lua_tonumber(L, -1));
                  lua_pop(L, 1);

                  lua_rawgeti(L, -1, 7);
                  fr.cy = static_cast<float>(lua_tonumber(L, -1));
                  lua_pop(L, 1);

                  lua_rawgeti(L, -1, 8);
                  fr.cw = static_cast<float>(lua_tonumber(L, -1));
                  lua_pop(L, 1);

                  lua_rawgeti(L, -1, 9);
                  fr.ch = static_cast<float>(lua_tonumber(L, -1));
                  lua_pop(L, 1);

                  fr.collidable = true;
                } else {
                  lua_pop(L, 1);
                }

                ++c.count;
              }

              lua_pop(L, 1);
            }
          }

          ++a.clip_count;
          lua_pop(L, 1);
        }

        bool collidable = false;
        for (uint8_t ci = 0; ci < a.clip_count && !collidable; ++ci)
          for (uint8_t fi = 0; fi < a.clips[ci].count && !collidable; ++fi)
            collidable = a.clips[ci].frames[fi].collidable;

        if (collidable) {
          b2BodyDef bdef = b2DefaultBodyDef();
          bdef.type = b2_kinematicBody;
          bdef.userData = reinterpret_cast<void*>(static_cast<uintptr_t>(entity));
          const auto id = b2CreateBody(_world, &bdef);
          _registry.emplace<body>(entity, id);
          _registry.emplace<boundary>(entity);
        }
      }

      lua_pop(L, 2);

      lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);
      lua_getfield(L, -1, "on_spawn");

      if (lua_isfunction(L, -1)) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, handle);

        if (lua_pcall(L, 1, 0, 0) != 0) [[unlikely]] {
          std::string error = lua_tostring(L, -1);
          lua_pop(L, 1);
          throw std::runtime_error(error);
        }
      } else {
        lua_pop(L, 1);
      }

      lua_pop(L, 1);

      lua_rawgeti(L, LUA_REGISTRYINDEX, handle);
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
    for (auto&& [en, ph, an, tf] :
         _registry.view<body, animation, transform>().each()) {
      if (!an.playing || an.clip_count == 0) {
        if (b2Shape_IsValid(ph.shape)) {
          b2DestroyShape(ph.shape, false);
          ph.shape = b2_nullShapeId;
          ph.cached_hx = 0.0f;
          ph.cached_hy = 0.0f;
        }

        continue;
      }

      const auto& frame = an.clips[an.active].frames[an.current];

      if (!frame.collidable || tf.alpha == 0 ||
          frame.cw <= 0.0f || frame.ch <= 0.0f) {
        if (b2Shape_IsValid(ph.shape)) {
          b2DestroyShape(ph.shape, false);
          ph.shape = b2_nullShapeId;
          ph.cached_hx = 0.0f;
          ph.cached_hy = 0.0f;
        }

        continue;
      }

      const auto hx = frame.cw * tf.scale * 0.5f;
      const auto hy = frame.ch * tf.scale * 0.5f;

      if (hx != ph.cached_hx || hy != ph.cached_hy) {
        if (b2Shape_IsValid(ph.shape))
          b2DestroyShape(ph.shape, false);

        const auto polygon = b2MakeBox(hx, hy);
        auto sdef = b2DefaultShapeDef();
        sdef.isSensor = true;
        sdef.enableSensorEvents = true;
        sdef.userData =
            reinterpret_cast<void*>(static_cast<uintptr_t>(en));
        ph.shape =
            b2CreatePolygonShape(ph.id, &sdef, &polygon);
        ph.cached_hx = hx;
        ph.cached_hy = hy;
      }

      const auto cx = tf.x + frame.cx * tf.scale + hx;
      const auto cy = tf.y + frame.cy * tf.scale + hy;
      const auto radians = tf.angle * (std::numbers::pi_v<float> / 180.0f);
      b2Body_SetTransform(ph.id, {cx, cy}, b2MakeRot(radians));
    }

    b2World_Step(_world, FIXED_TIMESTEP, WORLD_SUBSTEPS);

    const auto events = b2World_GetSensorEvents(_world);

    for (int i = 0; i < events.beginCount; ++i) {
      const auto& event = events.beginEvents[i];
      if (!b2Shape_IsValid(event.sensorShapeId) || !b2Shape_IsValid(event.visitorShapeId))
        continue;
      const auto sensor = static_cast<entt::entity>(
          reinterpret_cast<uintptr_t>(b2Shape_GetUserData(event.sensorShapeId)));
      const auto visitor = static_cast<entt::entity>(
          reinterpret_cast<uintptr_t>(b2Shape_GetUserData(event.visitorShapeId)));
      dispatch_collision(sensor, visitor, "on_collision_begin");
    }

    for (int i = 0; i < events.endCount; ++i) {
      const auto& event = events.endEvents[i];
      if (!b2Shape_IsValid(event.sensorShapeId) || !b2Shape_IsValid(event.visitorShapeId))
        continue;
      const auto sensor = static_cast<entt::entity>(
          reinterpret_cast<uintptr_t>(b2Shape_GetUserData(event.sensorShapeId)));
      const auto visitor = static_cast<entt::entity>(
          reinterpret_cast<uintptr_t>(b2Shape_GetUserData(event.visitorShapeId)));
      dispatch_collision(sensor, visitor, "on_collision_end");
    }

    {
      static constexpr std::string_view directions[] = {"left", "right", "top", "bottom"};

      for (auto&& [entity, sb, ph, proxy] :
           _registry.view<boundary, const body, const objectproxy>().each()) {
        if (!b2Shape_IsValid(ph.shape))
          continue;

        const auto aabb = b2Shape_GetAABB(ph.shape);

        uint8_t current = 0;
        if (aabb.upperBound.x < 0.0f)
          current |= boundary::left;
        if (aabb.lowerBound.x > viewport.width)
          current |= boundary::right;
        if (aabb.upperBound.y < 0.0f)
          current |= boundary::top;
        if (aabb.lowerBound.y > viewport.height)
          current |= boundary::bottom;

        const auto exited = static_cast<uint8_t>(current & ~sb.previous);
        const auto entered = static_cast<uint8_t>(sb.previous & ~current);

        for (uint8_t bit = 0; bit < 4; ++bit) {
          const auto mask = static_cast<uint8_t>(1u << bit);
          if (exited & mask)
            dispatch_screen_event(proxy, "on_screen_exit", directions[bit]);
          if (entered & mask)
            dispatch_screen_event(proxy, "on_screen_enter", directions[bit]);
        }

        sb.previous = current;
      }
    }

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

          if (proxy.prototype != LUA_NOREF && proxy.handle != LUA_NOREF) {
            lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.prototype);
            lua_getfield(L, -1, "on_animation_end");

            if (lua_isfunction(L, -1)) {
              lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.handle);
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

            lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.prototype);
            lua_getfield(L, -1, "on_animation_begin");

            if (lua_isfunction(L, -1)) {
              lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.handle);
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

  for (auto&& [entity, proxy] : _registry.view<objectproxy>().each()) {
    if (proxy.prototype == LUA_NOREF || proxy.handle == LUA_NOREF)
      continue;

    lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.prototype);
    lua_getfield(L, -1, "on_loop");

    if (lua_isfunction(L, -1)) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.handle);
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

void stage::dispatch_screen_event(
  const objectproxy& proxy,
  const char* callback,
  std::string_view direction
) {
  if (proxy.prototype == LUA_NOREF ||
      proxy.handle == LUA_NOREF)
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.prototype);
  lua_getfield(L, -1, callback);

  if (lua_isfunction(L, -1)) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.handle);
    lua_pushlstring(L, direction.data(), direction.size());

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

void stage::dispatch_collision(
  entt::entity entity,
  entt::entity other,
  const char* callback
) {
  if (!_registry.valid(entity) || !_registry.valid(other)) return;
  if (!_registry.all_of<objectproxy>(entity) ||
      !_registry.all_of<objectproxy>(other)) return;

  const auto& self = _registry.get<objectproxy>(entity);
  const auto& target = _registry.get<objectproxy>(other);

  if (self.prototype == LUA_NOREF ||
      self.handle == LUA_NOREF) return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, self.prototype);
  lua_getfield(L, -1, callback);

  if (lua_isfunction(L, -1)) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, self.handle);
    lua_pushstring(L, _strings->get(target.name));
    lua_pushstring(L, _strings->get(target.kind));

    if (lua_pcall(L, 3, 0, 0) != 0) [[unlikely]] {
      std::string error = lua_tostring(L, -1);
      lua_pop(L, 1);
      throw std::runtime_error(error);
    }
  } else {
    lua_pop(L, 1);
  }

  lua_pop(L, 1);
}

int stage::raycast(lua_State* state, entt::entity caller, float x, float y, float angle, float distance) {
  struct hit {
    entt::entity entity;
    float fraction;
  };

  std::vector<hit> hits;
  hits.reserve(16);

  const auto radians = angle * (std::numbers::pi_v<float> / 180.0f);
  const b2Vec2 origin{x, y};
  const b2Vec2 translation{std::cos(radians) * distance, std::sin(radians) * distance};
  const auto filter = b2DefaultQueryFilter();

  b2World_CastRay(
    _world,
    origin,
    translation,
    filter,
    [](b2ShapeId shape, b2Vec2, b2Vec2, float fraction, void* userdata) -> float {
      auto* results = static_cast<std::vector<hit>*>(userdata);
      const auto entity = static_cast<entt::entity>(
        reinterpret_cast<uintptr_t>(b2Shape_GetUserData(shape)));
      results->push_back({entity, fraction});
      return 1.0f;
    },
    &hits
  );

  std::ranges::sort(hits, {}, &hit::fraction);

  lua_newtable(state);
  int index = 1;

  for (const auto& [entity, fraction] : hits) {
    if (entity == caller)
      continue;

    if (!_registry.valid(entity) || !_registry.all_of<objectproxy>(entity))
      continue;

    const auto& proxy = _registry.get<objectproxy>(entity);
    if (proxy.handle == LUA_NOREF)
      continue;

    lua_rawgeti(state, LUA_REGISTRYINDEX, proxy.handle);
    lua_rawseti(state, -2, index++);
  }

  return 1;
}
