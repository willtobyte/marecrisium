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

stage::stage(std::string_view name, pixmappool& pixmaps, soundpool& sounds, sourcepool& sources)
    : _name(name),
      _pixmappool(pixmaps),
      _soundpool(sounds),
      _sourcepool(sources),
      _stringpool(std::make_unique<stringpool>()) {
  b2SetLengthUnitsPerMeter(100.f);

  b2WorldDef def = b2DefaultWorldDef();
  def.gravity = gravity;
  _world = b2CreateWorld(&def);

  _registry.on_destroy<objectproxy>().connect<&objectproxy::on_destroy>();
  _registry.on_destroy<body>().connect<[](entt::registry& registry, entt::entity entity) {
    auto& bo = registry.get<body>(entity);
    if (b2Body_IsValid(bo.id))
      b2DestroyBody(bo.id);
  }>();

  _registry.ctx().emplace<sourcepool*>(&_sourcepool);
  _registry.ctx().emplace<stringpool*>(_stringpool.get());

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
      const auto& proxy = _registry.emplace<objectproxy>(entity, _registry, entity, object_name, object_kind, _environment_reference);
      const auto prototype = proxy.prototype;
      const auto handle = proxy.handle;

      _stringpool->insert(object_name);
      _stringpool->insert(object_kind);

      lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);
      lua_getfield(L, -1, "animation");

      if (lua_istable(L, -1)) {
        auto& a = _registry.emplace<animation>(entity);
        a.pixmap = &_pixmappool.get(std::format("objects/{}", object_kind));

        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
          if (a.clip_count >= a.clips.size()) {
            lua_pop(L, 2);
            break;
          }

          const std::string_view clip_name = lua_tostring(L, -2);
          const auto cid = _stringpool->insert(clip_name);

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
                fr.duration = static_cast<float>(lua_tonumber(L, -1)) / 1000.f;
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
          lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);
          lua_getfield(L, -1, "body");
          const auto str = lua_isstring(L, -1)
            ? std::string_view{lua_tostring(L, -1)}
            : std::string_view{};
          lua_pop(L, 2);

          const auto bt = str == "dynamic" ? body_type::dynamic
                        : str == "static"  ? body_type::fixed
                                           : body_type::kinematic;

          b2BodyDef bdef = b2DefaultBodyDef();
          bdef.userData = reinterpret_cast<void*>(static_cast<uintptr_t>(entity));

          switch (bt) {
            case body_type::dynamic: {
              bdef.type = b2_dynamicBody;
              bdef.isBullet = true;
              bdef.fixedRotation = true;
            } break;
            case body_type::fixed: {
              bdef.type = b2_staticBody;
            } break;
            case body_type::kinematic: {
              bdef.type = b2_kinematicBody;
            } break;
          }

          const auto id = b2CreateBody(_world, &bdef);
          _registry.emplace<body>(entity, id, b2_nullShapeId, .0f, .0f, bt);
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

  lua_getfield(L, -1, "sounds");
  if (lua_istable(L, -1)) {
    if (_pool_reference == LUA_NOREF) {
      lua_newtable(L);
      _pool_reference = luaL_ref(L, LUA_REGISTRYINDEX);

      lua_rawgeti(L, LUA_REGISTRYINDEX, _environment_reference);
      lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_reference);
      lua_setfield(L, -2, "pool");
      lua_pop(L, 1);
    }

    const auto scount = static_cast<int>(lua_objlen(L, -1));

    for (int s = 1; s <= scount; ++s) {
      lua_rawgeti(L, -1, s);

      if (lua_isstring(L, -1)) {
        const std::string_view sname = lua_tostring(L, -1);

        auto& fx = _soundpool.get(std::format("sounds/{}", sname));
        auto** memory = static_cast<sound**>(lua_newuserdata(L, sizeof(sound*)));
        *memory = &fx;
        luaL_getmetatable(L, "Sound");
        lua_setmetatable(L, -2);

        lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_reference);
        lua_pushvalue(L, -2);
        lua_setfield(L, -2, sname.data());
        lua_pop(L, 1);

        _sounds.emplace_back(&fx);
        lua_pop(L, 1);
      }

      lua_pop(L, 1);
    }
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
  float wx, wy;
  const auto buttons = SDL_GetMouseState(&wx, &wy);
  SDL_RenderCoordinatesFromWindow(renderer, wx, wy, &wx, &wy);

  const auto released = _mouse_previous_buttons & ~buttons;
  _mouse_previous_buttons = buttons;

  if (released & SDL_BUTTON_LMASK) [[unlikely]]
    dispatch_click(wx, wy, "left");
  else if (released & SDL_BUTTON_MMASK) [[unlikely]]
    dispatch_click(wx, wy, "middle");
  else if (released & SDL_BUTTON_RMASK) [[unlikely]]
    dispatch_click(wx, wy, "right");

  dispatch_hover(wx, wy);

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

  _accumulator += delta;

  while (_accumulator >= FIXED_TIMESTEP) {
    for (auto&& [en, bd, an, tf] :
         _registry.view<body, animation, transform>().each()) {
      if (!an.playing || an.clip_count == 0) {
        if (b2Shape_IsValid(bd.shape)) {
          b2DestroyShape(bd.shape, false);
          bd.shape = b2_nullShapeId;
          bd.cached_hx = .0f;
          bd.cached_hy = .0f;
        }

        continue;
      }

      const auto& frame = an.clips[an.active].frames[an.current];

      if (!frame.collidable || tf.alpha <= .0f ||
          frame.cw <= .0f || frame.ch <= .0f) {
        if (b2Shape_IsValid(bd.shape)) {
          b2DestroyShape(bd.shape, false);
          bd.shape = b2_nullShapeId;
          bd.cached_hx = .0f;
          bd.cached_hy = .0f;
        }

        continue;
      }

      const auto hx = frame.cw * tf.scale * 0.5f;
      const auto hy = frame.ch * tf.scale * 0.5f;

      if (hx != bd.cached_hx || hy != bd.cached_hy) {
        const auto polygon = b2MakeBox(hx, hy);

        if (b2Shape_IsValid(bd.shape)) {
          b2Shape_SetPolygon(bd.shape, &polygon);

          if (bd.type == body_type::dynamic)
            b2Body_ApplyMassFromShapes(bd.id);
        } else {
          auto sdef = b2DefaultShapeDef();
          sdef.userData =
              reinterpret_cast<void*>(static_cast<uintptr_t>(en));

          switch (bd.type) {
            case body_type::kinematic: {
              sdef.isSensor = true;
              sdef.enableSensorEvents = true;
            } break;
            case body_type::dynamic: {
              sdef.enableContactEvents = true;
              sdef.enableSensorEvents = true;
              sdef.density = 1.f;
            } break;
            case body_type::fixed: {
              sdef.enableContactEvents = true;
              sdef.enableSensorEvents = true;
            } break;
          }

          bd.shape =
              b2CreatePolygonShape(bd.id, &sdef, &polygon);
        }

        bd.cached_hx = hx;
        bd.cached_hy = hy;
      }

      switch (bd.type) {
        case body_type::kinematic: {
          const auto cx = tf.x + frame.cx * tf.scale + hx;
          const auto cy = tf.y + frame.cy * tf.scale + hy;
          const auto radians = tf.angle * (std::numbers::pi_v<float> / 180.f);
          b2Body_SetTargetTransform(bd.id, {{cx, cy}, b2MakeRot(radians)}, FIXED_TIMESTEP);
        } break;
        case body_type::fixed: {
          const auto cx = tf.x + frame.cx * tf.scale + hx;
          const auto cy = tf.y + frame.cy * tf.scale + hy;
          const auto radians = tf.angle * (std::numbers::pi_v<float> / 180.f);
          b2Body_SetTransform(bd.id, {cx, cy}, b2MakeRot(radians));
        } break;
        case body_type::dynamic:
          break;
      }
    }

    b2World_Step(_world, FIXED_TIMESTEP, WORLD_SUBSTEPS);

    const auto body_events = b2World_GetBodyEvents(_world);

    for (int i = 0; i < body_events.moveCount; ++i) {
      const auto& event = body_events.moveEvents[i];
      const auto entity = static_cast<entt::entity>(
          reinterpret_cast<uintptr_t>(event.userData));

      if (!_registry.valid(entity)) [[unlikely]]
        continue;

      const auto* bd = _registry.try_get<body>(entity);
      if (!bd || bd->type != body_type::dynamic)
        continue;

      const auto* an = _registry.try_get<animation>(entity);
      if (!an || !an->playing || an->clip_count == 0)
        continue;

      auto& tf = _registry.get<transform>(entity);
      const auto& frame = an->clips[an->active].frames[an->current];
      const auto position = event.transform.p;
      tf.x = position.x - frame.cx * tf.scale - bd->cached_hx;
      tf.y = position.y - frame.cy * tf.scale - bd->cached_hy;
    }

    const auto sensor_events = b2World_GetSensorEvents(_world);

    for (int i = 0; i < sensor_events.beginCount; ++i) {
      const auto& event = sensor_events.beginEvents[i];
      if (!b2Shape_IsValid(event.sensorShapeId) || !b2Shape_IsValid(event.visitorShapeId))
        continue;
      const auto sensor = static_cast<entt::entity>(
          reinterpret_cast<uintptr_t>(b2Shape_GetUserData(event.sensorShapeId)));
      const auto visitor = static_cast<entt::entity>(
          reinterpret_cast<uintptr_t>(b2Shape_GetUserData(event.visitorShapeId)));
      dispatch_collision(sensor, visitor, "on_collision_begin");
    }

    for (int i = 0; i < sensor_events.endCount; ++i) {
      const auto& event = sensor_events.endEvents[i];
      if (!b2Shape_IsValid(event.sensorShapeId) || !b2Shape_IsValid(event.visitorShapeId))
        continue;
      const auto sensor = static_cast<entt::entity>(
          reinterpret_cast<uintptr_t>(b2Shape_GetUserData(event.sensorShapeId)));
      const auto visitor = static_cast<entt::entity>(
          reinterpret_cast<uintptr_t>(b2Shape_GetUserData(event.visitorShapeId)));
      dispatch_collision(sensor, visitor, "on_collision_end");
    }

    const auto contact_events = b2World_GetContactEvents(_world);

    for (int i = 0; i < contact_events.beginCount; ++i) {
      const auto& event = contact_events.beginEvents[i];
      if (!b2Shape_IsValid(event.shapeIdA) || !b2Shape_IsValid(event.shapeIdB))
        continue;
      const auto entity_a = static_cast<entt::entity>(
          reinterpret_cast<uintptr_t>(b2Shape_GetUserData(event.shapeIdA)));
      const auto entity_b = static_cast<entt::entity>(
          reinterpret_cast<uintptr_t>(b2Shape_GetUserData(event.shapeIdB)));
      dispatch_collision(entity_a, entity_b, "on_collision_begin");
      dispatch_collision(entity_b, entity_a, "on_collision_begin");
    }

    for (int i = 0; i < contact_events.endCount; ++i) {
      const auto& event = contact_events.endEvents[i];
      if (!b2Shape_IsValid(event.shapeIdA) || !b2Shape_IsValid(event.shapeIdB))
        continue;
      const auto entity_a = static_cast<entt::entity>(
          reinterpret_cast<uintptr_t>(b2Shape_GetUserData(event.shapeIdA)));
      const auto entity_b = static_cast<entt::entity>(
          reinterpret_cast<uintptr_t>(b2Shape_GetUserData(event.shapeIdB)));
      dispatch_collision(entity_a, entity_b, "on_collision_end");
      dispatch_collision(entity_b, entity_a, "on_collision_end");
    }

    {
      static constexpr std::string_view directions[] = {"left", "right", "top", "bottom"};

      for (auto&& [entity, sb, bd, proxy] :
           _registry.view<boundary, const body, const objectproxy>().each()) {
        if (!b2Shape_IsValid(bd.shape))
          continue;

        const auto aabb = b2Shape_GetAABB(bd.shape);

        uint8_t current = 0;
        if (aabb.upperBound.x < .0f)
          current |= boundary::left;
        if (aabb.lowerBound.x > viewport.width)
          current |= boundary::right;
        if (aabb.upperBound.y < .0f)
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

    if (fr.duration < .0f)
      continue;

    a.elapsed += delta;

    while (a.elapsed >= fr.duration && fr.duration > .0f) {
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
              lua_pushstring(L, _stringpool->get(c.name));

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
              lua_pushstring(L, _stringpool->get(c.name));

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

  for (auto* fx : _sounds) {
    fx->poll();
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

    if (tf.x + dw < .0f || tf.x > viewport.width ||
        tf.y + dh < .0f || tf.y > viewport.height)
      continue;

    a.pixmap->draw(
      fr.x, fr.y, fr.w, fr.h,
      tf.x, tf.y, dw, dh,
      static_cast<double>(tf.angle),
      static_cast<uint8_t>(std::clamp(tf.alpha, .0f, 255.0f))
    );
  }

#ifdef DEBUG
  SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);

  const b2AABB aabb = {{0, 0}, {viewport.width, viewport.height}};
  const b2QueryFilter filter = b2DefaultQueryFilter();

  b2World_OverlapAABB(_world, aabb, filter, [](b2ShapeId shape, void*) -> bool {
    const auto body_id = b2Shape_GetBody(shape);
    const auto xf = b2Body_GetTransform(body_id);
    const auto polygon = b2Shape_GetPolygon(shape);

    for (int i = 0; i < polygon.count; ++i) {
      const auto a = b2TransformPoint(xf, polygon.vertices[i]);
      const auto b = b2TransformPoint(xf, polygon.vertices[(i + 1) % polygon.count]);
      SDL_RenderLine(renderer, a.x, a.y, b.x, b.y);
    }

    return true;
  }, nullptr);

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
#endif
}

void stage::dispatch_click(float x, float y, const char* button) {
  constexpr auto HALF = .5f;
  const b2AABB aabb = {{x - HALF, y - HALF}, {x + HALF, y + HALF}};
  const auto filter = b2DefaultQueryFilter();

  struct context {
    entt::entity* hits;
    uint8_t count;
  };

  std::array<entt::entity, 32> buffer{};
  context ctx{buffer.data(), 0};

  b2World_OverlapAABB(
    _world, aabb, filter,
    [](b2ShapeId shape, void* userdata) -> bool {
      auto* ctx = static_cast<context*>(userdata);
      if (ctx->count >= 32) [[unlikely]]
        return false;
      const auto entity = static_cast<entt::entity>(
        reinterpret_cast<uintptr_t>(b2Shape_GetUserData(shape)));
      ctx->hits[ctx->count++] = entity;
      return true;
    },
    &ctx);

  if (ctx.count == 0) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
    lua_getfield(L, -1, "on_click");

    if (lua_isfunction(L, -1)) {
      lua_pushnumber(L, static_cast<lua_Number>(x));
      lua_pushnumber(L, static_cast<lua_Number>(y));
      lua_pushstring(L, button);

      if (lua_pcall(L, 3, 0, 0) != 0) [[unlikely]] {
        std::string error = lua_tostring(L, -1);
        lua_pop(L, 1);
        throw std::runtime_error(error);
      }
    } else {
      lua_pop(L, 1);
    }

    lua_pop(L, 1);
    return;
  }

  const auto span = std::span{buffer.data(), ctx.count};
  entt::entity topmost = entt::null;

  for (auto&& [entity, a, tf] : _registry.view<animation, transform>().each()) {
    if (!tf.shown || tf.alpha <= .0f) [[unlikely]]
      continue;

    for (const auto hit : span) {
      if (hit == entity) {
        topmost = entity;
        break;
      }
    }
  }

  if (topmost == entt::null) [[unlikely]]
    return;

  if (!_registry.all_of<objectproxy>(topmost)) [[unlikely]]
    return;

  const auto& proxy = _registry.get<objectproxy>(topmost);
  if (proxy.prototype == LUA_NOREF || proxy.handle == LUA_NOREF) [[unlikely]]
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.prototype);
  lua_getfield(L, -1, "on_click");

  if (lua_isfunction(L, -1)) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.handle);
    lua_pushnumber(L, static_cast<lua_Number>(x));
    lua_pushnumber(L, static_cast<lua_Number>(y));
    lua_pushstring(L, button);

    if (lua_pcall(L, 4, 0, 0) != 0) [[unlikely]] {
      std::string error = lua_tostring(L, -1);
      lua_pop(L, 1);
      throw std::runtime_error(error);
    }
  } else {
    lua_pop(L, 1);
  }

  lua_pop(L, 1);
}

void stage::dispatch_hover(float x, float y) {
  constexpr auto HALF = .5f;
  const b2AABB aabb = {{x - HALF, y - HALF}, {x + HALF, y + HALF}};
  const auto filter = b2DefaultQueryFilter();

  _hits.clear();

  b2World_OverlapAABB(
    _world, aabb, filter,
    [](b2ShapeId shape, void* userdata) -> bool {
      auto* hits = static_cast<std::unordered_set<entt::entity>*>(userdata);
      const auto entity = static_cast<entt::entity>(
        reinterpret_cast<uintptr_t>(b2Shape_GetUserData(shape)));
      hits->emplace(entity);
      return true;
    },
    &_hits);

  dispatch_unhover();

  for (const auto entity : _hits) {
    if (_hovering.contains(entity))
      continue;

    if (!_registry.valid(entity) || !_registry.all_of<objectproxy>(entity))
      continue;

    const auto& proxy = _registry.get<objectproxy>(entity);
    if (proxy.prototype == LUA_NOREF || proxy.handle == LUA_NOREF)
      continue;

    lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.prototype);
    lua_getfield(L, -1, "on_hover");

    if (lua_isfunction(L, -1)) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.handle);

      if (lua_pcall(L, 1, 0, 0) != 0) [[unlikely]] {
        std::string error = lua_tostring(L, -1);
        lua_pop(L, 1);
        throw std::runtime_error(error);
      }
    } else {
      lua_pop(L, 1);
    }

    lua_pop(L, 1);
  }

  _hovering.swap(_hits);
}

void stage::dispatch_unhover() {
  for (const auto entity : _hovering) {
    if (_hits.contains(entity))
      continue;

    if (!_registry.valid(entity) || !_registry.all_of<objectproxy>(entity))
      continue;

    const auto& proxy = _registry.get<objectproxy>(entity);
    if (proxy.prototype == LUA_NOREF || proxy.handle == LUA_NOREF)
      continue;

    lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.prototype);
    lua_getfield(L, -1, "on_unhover");

    if (lua_isfunction(L, -1)) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.handle);

      if (lua_pcall(L, 1, 0, 0) != 0) [[unlikely]] {
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
    lua_pushstring(L, _stringpool->get(target.name));
    lua_pushstring(L, _stringpool->get(target.kind));

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

  const auto radians = angle * (std::numbers::pi_v<float> / 180.f);
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
