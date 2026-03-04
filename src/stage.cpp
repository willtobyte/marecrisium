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
  _registry.on_destroy<body>().connect<[](entt::registry& registry, entt::entity entity) {
    auto& physics_body = registry.get<body>(entity);
    if (b2Body_IsValid(physics_body.id))
      b2DestroyBody(physics_body.id);
  }>();

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

      lua_getfield(L, -1, "name");
      const std::string_view object_name = lua_tostring(L, -1);
      lua_pop(L, 1);

      lua_getfield(L, -1, "kind");
      const std::string_view object_kind = lua_tostring(L, -1);
      lua_pop(L, 1);

      lua_pop(L, 1);

      _pixmaps->load(_name, object_kind);
      _sources->load(_name, object_kind);

      const auto entity = _registry.create();
      _registry.emplace<transform>(entity);
      const auto& proxy = _registry.emplace<objectproxy>(entity, _registry, entity, _name, object_name, object_kind, _environment_reference);
      const auto object_reference = proxy.object_reference;
      const auto self_reference = proxy.self_reference;

      const auto name_hash = entt::hashed_string{object_name.data()}.value();
      const auto kind_hash = entt::hashed_string{object_kind.data()}.value();
      _strings->insert(name_hash, object_name);
      _strings->insert(kind_hash, object_kind);

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

        bool needs_body = false;
        for (uint8_t clip_index = 0; clip_index < a.clip_count && !needs_body; ++clip_index)
          for (uint8_t frame_index = 0; frame_index < a.clips[clip_index].count && !needs_body; ++frame_index)
            needs_body = a.clips[clip_index].frames[frame_index].collidable;

        if (needs_body) {
          b2BodyDef body_definition = b2DefaultBodyDef();
          body_definition.type = b2_kinematicBody;
          body_definition.userData = reinterpret_cast<void*>(static_cast<uintptr_t>(entity));
          const auto body_id = b2CreateBody(_world, &body_definition);
          _registry.emplace<body>(entity, body_id);
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

      const auto& current_frame = an.clips[an.active].frames[an.current];

      if (!current_frame.collidable || tf.alpha == 0 ||
          current_frame.cw <= 0.0f || current_frame.ch <= 0.0f) {
        if (b2Shape_IsValid(ph.shape)) {
          b2DestroyShape(ph.shape, false);
          ph.shape = b2_nullShapeId;
          ph.cached_hx = 0.0f;
          ph.cached_hy = 0.0f;
        }

        continue;
      }

      const auto half_width = current_frame.cw * tf.scale * 0.5f;
      const auto half_height = current_frame.ch * tf.scale * 0.5f;

      if (half_width != ph.cached_hx || half_height != ph.cached_hy) {
        if (b2Shape_IsValid(ph.shape))
          b2DestroyShape(ph.shape, false);

        const auto polygon = b2MakeBox(half_width, half_height);
        auto shape_definition = b2DefaultShapeDef();
        shape_definition.isSensor = true;
        shape_definition.enableSensorEvents = true;
        shape_definition.userData =
            reinterpret_cast<void*>(static_cast<uintptr_t>(en));
        ph.shape =
            b2CreatePolygonShape(ph.id, &shape_definition, &polygon);
        ph.cached_hx = half_width;
        ph.cached_hy = half_height;
      }

      const auto center_x = tf.x + current_frame.cx * tf.scale + half_width;
      const auto center_y = tf.y + current_frame.cy * tf.scale + half_height;
      const auto radians = tf.angle * (std::numbers::pi_v<float> / 180.0f);
      b2Body_SetTransform(ph.id, {center_x, center_y}, b2MakeRot(radians));
    }

    b2World_Step(_world, FIXED_TIMESTEP, WORLD_SUBSTEPS);

    const auto sensor_events = b2World_GetSensorEvents(_world);

    for (int i = 0; i < sensor_events.beginCount; ++i) {
      const auto& event = sensor_events.beginEvents[i];
      const auto sensor_entity = static_cast<entt::entity>(
          reinterpret_cast<uintptr_t>(b2Shape_GetUserData(event.sensorShapeId)));
      const auto visitor_entity = static_cast<entt::entity>(
          reinterpret_cast<uintptr_t>(b2Shape_GetUserData(event.visitorShapeId)));
      dispatch_collision(sensor_entity, visitor_entity, "on_collision_begin");
    }

    for (int i = 0; i < sensor_events.endCount; ++i) {
      const auto& event = sensor_events.endEvents[i];
      const auto sensor_entity = static_cast<entt::entity>(
          reinterpret_cast<uintptr_t>(b2Shape_GetUserData(event.sensorShapeId)));
      const auto visitor_entity = static_cast<entt::entity>(
          reinterpret_cast<uintptr_t>(b2Shape_GetUserData(event.visitorShapeId)));
      dispatch_collision(sensor_entity, visitor_entity, "on_collision_end");
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

void stage::dispatch_collision(
  entt::entity entity,
  entt::entity other,
  const char* callback
) {
  if (!_registry.valid(entity) || !_registry.valid(other)) return;
  if (!_registry.all_of<objectproxy>(entity) ||
      !_registry.all_of<objectproxy>(other)) return;

  const auto& self_proxy = _registry.get<objectproxy>(entity);
  const auto& other_proxy = _registry.get<objectproxy>(other);

  if (self_proxy.object_reference == LUA_NOREF ||
      self_proxy.self_reference == LUA_NOREF) return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, self_proxy.object_reference);
  lua_getfield(L, -1, callback);

  if (lua_isfunction(L, -1)) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, self_proxy.self_reference);
    lua_pushstring(L, _strings->get(other_proxy.name));
    lua_pushstring(L, _strings->get(other_proxy.kind));

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
