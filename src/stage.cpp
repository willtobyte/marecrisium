#include "stage.hpp"

static void* to_userdata(entt::entity e) noexcept {
  return reinterpret_cast<void*>(static_cast<uintptr_t>(e) + 1);
}

static entt::entity to_entity(const void* p) noexcept {
  return static_cast<entt::entity>(reinterpret_cast<uintptr_t>(p) - 1);
}

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
    : _name(name) {
  _registry.on_destroy<objectproxy>().connect<&objectproxy::on_destroy>();
  _registry.on_destroy<body>().connect<[](entt::registry& registry, entt::entity entity) {
    auto& bo = registry.get<body>(entity);
    if (b2Body_IsValid(bo.id))
      b2DestroyBody(bo.id);
  }>();

  _registry.ctx().emplace<stringpool*>(&_stringpool);

  lua_newtable(L);
  lua_newtable(L);
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  lua_setfield(L, -2, "__index");
  lua_setmetatable(L, -2);
  _environment_reference = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_newtable(L);
  _pool_reference = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _environment_reference);
  lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_reference);
  lua_setfield(L, -2, "pool");
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
    throw std::runtime_error(std::move(error));
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, _environment_reference);
  lua_setfenv(L, -2);

  if (lua_pcall(L, 0, 1, 0) != 0) [[unlikely]] {
    std::string error = lua_tostring(L, -1);
    lua_pop(L, 1);
    throw std::runtime_error(std::move(error));
  }

  b2Vec2 gravity{.0f, .0f};
  lua_getfield(L, -1, "gravity");
  if (lua_istable(L, -1)) {
    lua_rawgeti(L, -1, 1);
    gravity.x = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : .0f;
    lua_pop(L, 1);

    lua_rawgeti(L, -1, 2);
    gravity.y = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : .0f;
    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  b2WorldDef def = b2DefaultWorldDef();
  def.gravity = gravity;
  _world = b2CreateWorld(&def);

  lua_getfield(L, -1, "objects");
  if (lua_istable(L, -1)) {
    const auto count = static_cast<int>(lua_objlen(L, -1));

    for (int i = 1; i <= count; ++i) {
      lua_rawgeti(L, -1, i);

      lua_getfield(L, -1, "name");
      const std::string object_name{lua_tostring(L, -1)};
      lua_pop(L, 1);

      lua_getfield(L, -1, "kind");
      const std::string object_kind{lua_tostring(L, -1)};
      lua_pop(L, 1);

      lua_pop(L, 1);

      const auto entity = _registry.create();
      _registry.emplace<transform>(entity);
      const auto& proxy = _registry.emplace<objectproxy>(entity, _registry, entity, object_name, object_kind, _environment_reference);
      const auto prototype = proxy.prototype;
      const auto handle = proxy.handle;

      static_cast<void>(_stringpool.insert(object_name));
      static_cast<void>(_stringpool.insert(object_kind));

      lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);
      lua_getfield(L, -1, "animation");

      if (lua_istable(L, -1)) {
        auto& a = _registry.emplace<animation>(entity);
        a.pixmap = &resources.pixmap.get(std::format("objects/{}", object_kind));

        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
          if (a.clip_count >= a.clips.size()) {
            lua_pop(L, 2);
            break;
          }

          const std::string_view clip_name = lua_tostring(L, -2);
          const auto cid = _stringpool.insert(clip_name);

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
          bdef.userData = to_userdata(entity);

          bdef.type = bt == body_type::dynamic   ? b2_dynamicBody
                   : bt == body_type::fixed     ? b2_staticBody
                                                : b2_kinematicBody;

          if (bt == body_type::dynamic) {
            bdef.isBullet = true;
            bdef.fixedRotation = true;
          }

          const auto id = b2CreateBody(_world, &bdef);
          _registry.emplace<body>(entity, id, b2_nullShapeId, .0f, .0f, bt);
          _registry.emplace<boundary>(entity);
        }
      }

      lua_pop(L, 2);

      {
        lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);
        lua_getfield(L, -1, "cullable");
        const auto wants_cullable = lua_toboolean(L, -1) != 0;
        lua_pop(L, 2);

        if (wants_cullable) {
          const auto* bd = _registry.try_get<body>(entity);
          if (!bd || bd->type != body_type::dynamic)
            _registry.emplace<cullable>(entity);
        }
      }

      lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);
      lua_getfield(L, -1, "on_spawn");

      if (lua_isfunction(L, -1)) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, handle);

        if (lua_pcall(L, 1, 0, 0) != 0) [[unlikely]] {
          std::string error = lua_tostring(L, -1);
          lua_pop(L, 1);
          throw std::runtime_error(std::move(error));
        }
      } else {
        lua_pop(L, 1);
      }

      lua_pop(L, 1);

      lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_reference);
      lua_rawgeti(L, LUA_REGISTRYINDEX, handle);
      lua_setfield(L, -2, object_name.data());
      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);

  lua_getfield(L, -1, "sounds");
  if (lua_istable(L, -1)) {
    const auto count = static_cast<int>(lua_objlen(L, -1));
    _sounds.reserve(static_cast<size_t>(count));

    for (auto i = 1; i <= count; ++i) {
      lua_rawgeti(L, -1, i);

      if (lua_isstring(L, -1)) {
        const std::string_view sound_name = lua_tostring(L, -1);

        auto& instance = resources.sound.get(std::format("sounds/{}", sound_name));
        auto** memory = static_cast<sound**>(lua_newuserdata(L, sizeof(sound*)));
        *memory = &instance;
        luaL_getmetatable(L, "Sound");
        lua_setmetatable(L, -2);

        lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_reference);
        lua_pushvalue(L, -2);
        lua_setfield(L, -2, sound_name.data());
        lua_pop(L, 1);

        _sounds.emplace_back(&instance);
        lua_pop(L, 1);
      }

      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);

  _backdrop = &resources.pixmap.get(std::format("stages/{}", _name));

  lua_getfield(L, -1, "tilemap");
  if (lua_isstring(L, -1)) {
    const std::string_view tilemap_name = lua_tostring(L, -1);
    _tilemap = tilemap(tilemap_name, _world);
  }
  lua_pop(L, 1);

  lua_getfield(L, -1, "particles");
  if (lua_istable(L, -1)) {
    const auto count = static_cast<int>(lua_objlen(L, -1));

    for (int i = 1; i <= count; ++i) {
      lua_rawgeti(L, -1, i);

      lua_getfield(L, -1, "name");
      const std::string particle_name{lua_tostring(L, -1)};
      lua_pop(L, 1);

      lua_getfield(L, -1, "kind");
      const std::string particle_kind{lua_tostring(L, -1)};
      lua_pop(L, 1);

      lua_getfield(L, -1, "x");
      const auto px = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : .0f;
      lua_pop(L, 1);

      lua_getfield(L, -1, "y");
      const auto py = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : .0f;
      lua_pop(L, 1);

      lua_getfield(L, -1, "active");
      const auto active = lua_isboolean(L, -1) ? lua_toboolean(L, -1) != 0 : true;
      lua_pop(L, 1);

      lua_pop(L, 1);

      auto& instance = _particlesystem.add(particle_name, particle_kind, px, py, active);
      auto** memory = static_cast<particle**>(lua_newuserdata(L, sizeof(particle*)));
      *memory = &instance;
      luaL_getmetatable(L, "Particle");
      lua_setmetatable(L, -2);

      lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_reference);
      lua_pushvalue(L, -2);
      lua_setfield(L, -2, particle_name.data());
      lua_pop(L, 1);

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
      throw std::runtime_error(std::move(error));
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
      throw std::runtime_error(std::move(error));
    }
  } else {
    lua_pop(L, 1);
  }

  lua_pop(L, 1);
}

void stage::on_tick(uint64_t tick) {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
  lua_getfield(L, -1, "on_tick");

  if (lua_isfunction(L, -1)) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
    lua_pushnumber(L, static_cast<lua_Number>(tick));

    if (lua_pcall(L, 2, 0, 0) != 0) [[unlikely]] {
      std::string error = lua_tostring(L, -1);
      lua_pop(L, 2);
      throw std::runtime_error(std::move(error));
    }
  } else {
    lua_pop(L, 1);
  }

  lua_pop(L, 1);
}

void stage::update(float delta) {
  static constexpr float CULLING_MARGIN = 32.f;

  for (auto&& [entity, proxy, tf, an] :
       _registry.view<cullable, objectproxy, transform, animation>().each()) {
    const auto& frame = an.clips[an.active].frames[an.current];
    const auto width = frame.w * tf.scale;
    const auto height = frame.h * tf.scale;
    const auto screen_x = tf.x - _camera_x;
    const auto screen_y = tf.y - _camera_y;

    const auto offscreen =
      screen_x + width < -CULLING_MARGIN ||
      screen_x > viewport.width + CULLING_MARGIN ||
      screen_y + height < -CULLING_MARGIN ||
      screen_y > viewport.height + CULLING_MARGIN;

    if (offscreen) {
      if (!_registry.all_of<dormant>(entity)) {
        _registry.emplace<dormant>(entity);

        if (auto* bd = _registry.try_get<body>(entity);
            bd && b2Shape_IsValid(bd->shape)) {
          b2DestroyShape(bd->shape, false);
          bd->shape = b2_nullShapeId;
          bd->cached_hx = .0f;
          bd->cached_hy = .0f;
        }

        dispatch_dormancy(proxy, "on_sleep");
      }
    } else {
      if (_registry.all_of<dormant>(entity)) {
        _registry.remove<dormant>(entity);
        dispatch_dormancy(proxy, "on_wake");
      }
    }
  }

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
      throw std::runtime_error(std::move(error));
    }
  } else {
    lua_pop(L, 1);
  }

  lua_pop(L, 1);

  for (auto&& [entity, proxy] : _registry.view<objectproxy>(entt::exclude<dormant>).each()) {
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
        throw std::runtime_error(std::move(error));
      }
    } else {
      lua_pop(L, 1);
    }

    lua_pop(L, 1);

    auto* a = _registry.try_get<animation>(entity);
    if (!a || !a->playing || a->clip_count == 0)
      continue;

    const auto& c = a->clips[a->active];
    if (c.count == 0)
      continue;

    const auto& fr = c.frames[a->current];

    if (fr.duration < .0f)
      continue;

    a->elapsed += delta;

    while (a->elapsed >= fr.duration && fr.duration > .0f) {
      a->elapsed -= fr.duration;
      ++a->current;

      if (a->current >= c.count) {
        a->current = 0;

        lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.prototype);
        lua_getfield(L, -1, "on_animation_end");

        if (lua_isfunction(L, -1)) {
          lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.handle);
          lua_pushstring(L, _stringpool.get(c.name));

          if (lua_pcall(L, 2, 0, 0) != 0) [[unlikely]] {
            std::string error = lua_tostring(L, -1);
            lua_pop(L, 2);
            throw std::runtime_error(std::move(error));
          }
        } else {
          lua_pop(L, 1);
        }

        lua_pop(L, 1);

        lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.prototype);
        lua_getfield(L, -1, "on_animation_begin");

        if (lua_isfunction(L, -1)) {
          lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.handle);
          lua_pushstring(L, _stringpool.get(c.name));

          if (lua_pcall(L, 2, 0, 0) != 0) [[unlikely]] {
            std::string error = lua_tostring(L, -1);
            lua_pop(L, 2);
            throw std::runtime_error(std::move(error));
          }
        } else {
          lua_pop(L, 1);
        }

        lua_pop(L, 1);
      }

      break;
    }
  }

  _accumulator += delta;

  while (_accumulator >= _timestep) {
    for (auto&& [en, bd, an, tf] :
         _registry.view<body, animation, transform>(entt::exclude<dormant>).each()) {
      const auto active = an.playing && an.clip_count > 0;
      const auto& frame = an.clips[an.active].frames[an.current];
      const auto visible = active && frame.collidable &&
                           tf.alpha > .0f && frame.cw > .0f && frame.ch > .0f;

      if (!visible) {
        if (b2Shape_IsValid(bd.shape)) {
          b2DestroyShape(bd.shape, false);
          bd.shape = b2_nullShapeId;
          bd.cached_hx = .0f;
          bd.cached_hy = .0f;
        }
        continue;
      }

      if (!b2Shape_IsValid(bd.shape)) {
        const auto hx = frame.cw * tf.scale * .5f;
        const auto hy = frame.ch * tf.scale * .5f;
        const auto polygon = b2MakeBox(hx, hy);

        auto sdef = b2DefaultShapeDef();
        sdef.userData = to_userdata(en);
        sdef.enableContactEvents = true;
        sdef.enableSensorEvents = true;

        if (bd.type == body_type::dynamic) {
          sdef.density = 1.f;
          sdef.material.friction = .0f;
        }

        bd.shape = b2CreatePolygonShape(bd.id, &sdef, &polygon);
        bd.cached_hx = hx;
        bd.cached_hy = hy;

        const auto cx = tf.x + frame.cx * tf.scale + hx;
        const auto cy = tf.y + frame.cy * tf.scale + hy;

        if (bd.type == body_type::kinematic) {
          b2Body_SetTargetTransform(bd.id, {{cx, cy}, b2MakeRot(to_radians(tf.angle))}, _timestep);
        } else {
          const auto rot = bd.type == body_type::dynamic ? b2MakeRot(.0f) : b2MakeRot(to_radians(tf.angle));
          b2Body_SetTransform(bd.id, {cx, cy}, rot);
        }

        continue;
      }

      if (bd.type == body_type::kinematic) {
        const auto cx = tf.x + frame.cx * tf.scale + bd.cached_hx;
        const auto cy = tf.y + frame.cy * tf.scale + bd.cached_hy;
        b2Body_SetTargetTransform(bd.id, {{cx, cy}, b2MakeRot(to_radians(tf.angle))}, _timestep);
      }
    }

    b2World_Step(_world, _timestep, _substeps);

    const auto body_events = b2World_GetBodyEvents(_world);

    for (int i = 0; i < body_events.moveCount; ++i) {
      const auto& event = body_events.moveEvents[i];
      const auto entity = to_entity(event.userData);

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

    static constexpr std::string_view directions[] = {"left", "right", "top", "bottom"};

    for (auto&& [entity, bd] :
         _registry.view<const body>(entt::exclude<dormant>).each()) {
      if (!b2Body_IsValid(bd.id))
        continue;

      if (bd.type == body_type::dynamic) {
        const auto capacity = b2Body_GetContactCapacity(bd.id);
        bool on_ground = false;
        entt::entity ride_target = entt::null;

        if (capacity > 0) {
          std::array<b2ContactData, 8> contacts{};
          const auto count = b2Body_GetContactData(bd.id, contacts.data(), static_cast<int>(contacts.size()));
          const auto* user = b2Shape_GetUserData(bd.shape);

          for (auto j = 0uz; j < static_cast<size_t>(count); ++j) {
            const auto& manifold = contacts[j].manifold;
            const auto is_shape_a = b2Shape_GetUserData(contacts[j].shapeIdA) == user;
            const auto normal_y = is_shape_a ? manifold.normal.y : -manifold.normal.y;
            if (normal_y > .5f) {
              on_ground = true;

              const auto other_shape = is_shape_a ? contacts[j].shapeIdB : contacts[j].shapeIdA;
              const auto other_body = b2Shape_GetBody(other_shape);
              if (b2Body_GetType(other_body) == b2_kinematicBody) {
                const auto* other_data = b2Shape_GetUserData(other_shape);
                if (other_data)
                  ride_target = to_entity(other_data);
              }

              break;
            }
          }
        }

        if (on_ground) {
          _registry.emplace_or_replace<grounded>(entity);
        } else {
          _registry.remove<grounded>(entity);
        }

        _registry.emplace_or_replace<riding>(entity, ride_target);
      }

      if (auto* sb = _registry.try_get<boundary>(entity);
          sb && b2Shape_IsValid(bd.shape)) {
        const auto* proxy = _registry.try_get<objectproxy>(entity);
        if (proxy) {
          const auto aabb = b2Shape_GetAABB(bd.shape);

          uint8_t current = 0;
          if (aabb.upperBound.x < _camera_x)
            current |= boundary::left;
          if (aabb.lowerBound.x > _camera_x + viewport.width)
            current |= boundary::right;
          if (aabb.upperBound.y < _camera_y)
            current |= boundary::top;
          if (aabb.lowerBound.y > _camera_y + viewport.height)
            current |= boundary::bottom;

          const auto exited = static_cast<uint8_t>(current & ~sb->previous);
          const auto entered = static_cast<uint8_t>(sb->previous & ~current);

          for (uint8_t bit = 0; bit < 4; ++bit) {
            const auto mask = static_cast<uint8_t>(1u << bit);
            if (exited & mask)
              dispatch_screen_event(*proxy, "on_screen_exit", directions[bit]);
            if (entered & mask)
              dispatch_screen_event(*proxy, "on_screen_enter", directions[bit]);
          }

          sb->previous = current;
        }
      }
    }

    const auto sensor_events = b2World_GetSensorEvents(_world);

    for (int i = 0; i < sensor_events.beginCount; ++i) {
      const auto& event = sensor_events.beginEvents[i];
      if (!b2Shape_IsValid(event.sensorShapeId) || !b2Shape_IsValid(event.visitorShapeId))
        continue;
      const auto* sensor_data = b2Shape_GetUserData(event.sensorShapeId);
      const auto* visitor_data = b2Shape_GetUserData(event.visitorShapeId);
      if (!sensor_data || !visitor_data) continue;
      const auto sensor = to_entity(sensor_data);
      const auto visitor = to_entity(visitor_data);
      dispatch_collision(sensor, visitor, "on_collision_begin");
    }

    for (int i = 0; i < sensor_events.endCount; ++i) {
      const auto& event = sensor_events.endEvents[i];
      if (!b2Shape_IsValid(event.sensorShapeId) || !b2Shape_IsValid(event.visitorShapeId))
        continue;
      const auto* sensor_data = b2Shape_GetUserData(event.sensorShapeId);
      const auto* visitor_data = b2Shape_GetUserData(event.visitorShapeId);
      if (!sensor_data || !visitor_data) continue;
      const auto sensor = to_entity(sensor_data);
      const auto visitor = to_entity(visitor_data);
      dispatch_collision(sensor, visitor, "on_collision_end");
    }

    const auto contact_events = b2World_GetContactEvents(_world);

    for (int i = 0; i < contact_events.beginCount; ++i) {
      const auto& event = contact_events.beginEvents[i];
      if (!b2Shape_IsValid(event.shapeIdA) || !b2Shape_IsValid(event.shapeIdB))
        continue;
      const auto* data_a = b2Shape_GetUserData(event.shapeIdA);
      const auto* data_b = b2Shape_GetUserData(event.shapeIdB);
      if (!data_a || !data_b) continue;
      const auto entity_a = to_entity(data_a);
      const auto entity_b = to_entity(data_b);
      const auto& normal = event.manifold.normal;
      const b2Vec2 flipped = {-normal.x, -normal.y};
      dispatch_collision(entity_a, entity_b, "on_collision_begin", &normal);
      dispatch_collision(entity_b, entity_a, "on_collision_begin", &flipped);
    }

    for (int i = 0; i < contact_events.endCount; ++i) {
      const auto& event = contact_events.endEvents[i];
      if (!b2Shape_IsValid(event.shapeIdA) || !b2Shape_IsValid(event.shapeIdB))
        continue;
      const auto* data_a = b2Shape_GetUserData(event.shapeIdA);
      const auto* data_b = b2Shape_GetUserData(event.shapeIdB);
      if (!data_a || !data_b) continue;
      const auto entity_a = to_entity(data_a);
      const auto entity_b = to_entity(data_b);
      dispatch_collision(entity_a, entity_b, "on_collision_end");
      dispatch_collision(entity_b, entity_a, "on_collision_end");
    }

    _accumulator -= _timestep;
  }

  for (auto* instance : _sounds) {
    instance->poll();
  }

  _particlesystem.update(delta);
}

void stage::draw() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
  lua_getfield(L, -1, "on_paint");

  _camera_x = .0f;
  _camera_y = .0f;

  if (lua_isfunction(L, -1)) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);

    if (lua_pcall(L, 1, 2, 0) != 0) [[unlikely]] {
      std::string error = lua_tostring(L, -1);
      lua_pop(L, 2);
      throw std::runtime_error(std::move(error));
    }

    if (lua_isnumber(L, -2))
      _camera_x = std::roundf(static_cast<float>(lua_tonumber(L, -2)));
    if (lua_isnumber(L, -1))
      _camera_y = std::roundf(static_cast<float>(lua_tonumber(L, -1)));
    lua_pop(L, 2);
  } else {
    lua_pop(L, 1);
  }

  lua_pop(L, 1);

  _backdrop->draw(
    .0f, .0f,
    static_cast<float>(_backdrop->width()),
    static_cast<float>(_backdrop->height()),
    .0f, .0f, viewport.width, viewport.height
  );

  _tilemap.set_camera(_camera_x, _camera_y, viewport.width, viewport.height);
  _tilemap.draw_background();

  for (auto&& [entity, a, tf] : _registry.view<animation, transform>(entt::exclude<dormant>).each()) {
    if (!tf.shown || !a.playing || !a.pixmap || a.clip_count == 0) [[unlikely]]
      continue;

    const auto& c = a.clips[a.active];
    if (c.count == 0)
      continue;

    const auto& fr = c.frames[a.current];

    const auto dw = fr.w * tf.scale;
    const auto dh = fr.h * tf.scale;
    const auto sx = tf.x - _camera_x;
    const auto sy = tf.y - _camera_y;

    if (sx + dw < .0f || sx > viewport.width ||
        sy + dh < .0f || sy > viewport.height)
      continue;

    a.pixmap->draw(
      fr.x, fr.y, fr.w, fr.h,
      sx, sy, dw, dh,
      static_cast<double>(tf.angle),
      static_cast<uint8_t>(std::clamp(tf.alpha, .0f, 255.f)),
      tf.flip
    );
  }

  _particlesystem.draw(_camera_x, _camera_y);

  _tilemap.draw_foreground();

#ifdef DEBUG
  SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);

  const b2AABB aabb = {{_camera_x, _camera_y}, {_camera_x + viewport.width, _camera_y + viewport.height}};
  const b2QueryFilter filter = b2DefaultQueryFilter();
  const float offset[] = {_camera_x, _camera_y};

  b2World_OverlapAABB(_world, aabb, filter, [](b2ShapeId shape, void* userdata) -> bool {
    const auto* offset = static_cast<const float*>(userdata);

    const auto shape_aabb = b2Shape_GetAABB(shape);
    const auto lx = shape_aabb.lowerBound.x - offset[0];
    const auto ly = shape_aabb.lowerBound.y - offset[1];
    const auto ux = shape_aabb.upperBound.x - offset[0];
    const auto uy = shape_aabb.upperBound.y - offset[1];

    if (ux < .0f || lx > viewport.width || uy < .0f || ly > viewport.height)
      return true;

    const auto bid = b2Shape_GetBody(shape);
    const auto xf = b2Body_GetTransform(bid);
    const auto polygon = b2Shape_GetPolygon(shape);

    for (int i = 0; i < polygon.count; ++i) {
      const auto a = b2TransformPoint(xf, polygon.vertices[i]);
      const auto b = b2TransformPoint(xf, polygon.vertices[(i + 1) % polygon.count]);

      SDL_RenderLine(renderer, a.x - offset[0], a.y - offset[1], b.x - offset[0], b.y - offset[1]);
    }

    return true;
  }, const_cast<float*>(offset));

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
      ctx->hits[ctx->count++] = to_entity(b2Shape_GetUserData(shape));
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
        lua_pop(L, 2);
        throw std::runtime_error(std::move(error));
      }
    } else {
      lua_pop(L, 1);
    }

    lua_pop(L, 1);
    return;
  }

  const auto span = std::span{buffer.data(), ctx.count};
  entt::entity topmost = entt::null;

  for (auto&& [entity, a, tf] : _registry.view<animation, transform>(entt::exclude<dormant>).each()) {
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
      lua_pop(L, 2);
      throw std::runtime_error(std::move(error));
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

  struct context {
    entt::entity* hits;
    uint8_t count;
  };

  std::array<entt::entity, 16> buffer{};
  context ctx{buffer.data(), 0};

  b2World_OverlapAABB(
    _world, aabb, filter,
    [](b2ShapeId shape, void* userdata) -> bool {
      auto* ctx = static_cast<context*>(userdata);
      if (ctx->count >= 16) [[unlikely]]
        return false;
      ctx->hits[ctx->count++] = to_entity(b2Shape_GetUserData(shape));
      return true;
    },
    &ctx);

  const auto hits = std::span{buffer.data(), ctx.count};
  std::ranges::sort(hits);

  dispatch_unhover(hits);

  for (const auto entity : hits) {
    if (std::ranges::binary_search(_hovering, entity))
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
        lua_pop(L, 2);
        throw std::runtime_error(std::move(error));
      }
    } else {
      lua_pop(L, 1);
    }

    lua_pop(L, 1);
  }

  _hovering.assign(hits.begin(), hits.end());
}

void stage::dispatch_unhover(std::span<const entt::entity> current) {
  for (const auto entity : _hovering) {
    if (std::ranges::binary_search(current, entity))
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
        lua_pop(L, 2);
        throw std::runtime_error(std::move(error));
      }
    } else {
      lua_pop(L, 1);
    }

    lua_pop(L, 1);
  }
}

void stage::dispatch_screen_event(const objectproxy& proxy, const char* callback, std::string_view direction) {
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
      lua_pop(L, 2);
      throw std::runtime_error(std::move(error));
    }
  } else {
    lua_pop(L, 1);
  }

  lua_pop(L, 1);
}

void stage::dispatch_dormancy(const objectproxy& proxy, const char* callback) {
  if (proxy.prototype == LUA_NOREF ||
      proxy.handle == LUA_NOREF)
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.prototype);
  lua_getfield(L, -1, callback);

  if (lua_isfunction(L, -1)) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.handle);

    if (lua_pcall(L, 1, 0, 0) != 0) [[unlikely]] {
      std::string error = lua_tostring(L, -1);
      lua_pop(L, 2);
      throw std::runtime_error(std::move(error));
    }
  } else {
    lua_pop(L, 1);
  }

  lua_pop(L, 1);
}

void stage::dispatch_collision(entt::entity entity, entt::entity other, const char* callback, const b2Vec2* normal) {
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
    lua_pushstring(L, _stringpool.get(target.name));
    lua_pushstring(L, _stringpool.get(target.kind));

    auto argc = 3;
    if (normal) {
      lua_pushnumber(L, static_cast<double>(normal->x));
      lua_pushnumber(L, static_cast<double>(normal->y));
      argc = 5;
    }

    if (lua_pcall(L, argc, 0, 0) != 0) [[unlikely]] {
      std::string error = lua_tostring(L, -1);
      lua_pop(L, 2);
      throw std::runtime_error(std::move(error));
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

  const auto radians = to_radians(angle);
  auto sine = .0f, cosine = .0f;
  sincos(radians, sine, cosine);
  const b2Vec2 origin{x, y};
  const b2Vec2 translation{cosine * distance, sine * distance};
  const auto filter = b2DefaultQueryFilter();

  b2World_CastRay(
    _world,
    origin,
    translation,
    filter,
    [](b2ShapeId shape, b2Vec2, b2Vec2, float fraction, void* userdata) -> float {
      auto* results = static_cast<std::vector<hit>*>(userdata);
      results->push_back({to_entity(b2Shape_GetUserData(shape)), fraction});
      return 1.f;
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
