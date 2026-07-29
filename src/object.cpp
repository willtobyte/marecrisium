namespace {
  namespace lookup {
    constexpr auto alive = "alive"_hs;
    constexpr auto x = "x"_hs;
    constexpr auto y = "y"_hs;
    constexpr auto z = "z"_hs;
    constexpr auto velocity_x = "velocity_x"_hs;
    constexpr auto velocity_y = "velocity_y"_hs;
    constexpr auto flip = "flip"_hs;
    constexpr auto dormant = "dormant"_hs;
    constexpr auto animation = "animation"_hs;
    constexpr auto shown = "shown"_hs;
    constexpr auto scale = "scale"_hs;
    constexpr auto angle = "angle"_hs;
    constexpr auto alpha = "alpha"_hs;
    constexpr auto name = "name"_hs;
    constexpr auto kind = "kind"_hs;
    constexpr auto center_x = "center_x"_hs;
    constexpr auto center_y = "center_y"_hs;
    constexpr auto body = "body"_hs;
    constexpr auto sleepable = "sleepable"_hs;
    constexpr auto on_spawn = "on_spawn"_hs;
    constexpr auto on_loop = "on_loop"_hs;
    constexpr auto on_sleep = "on_sleep"_hs;
    constexpr auto on_wake = "on_wake"_hs;
    constexpr auto on_screen_exit = "on_screen_exit"_hs;
    constexpr auto on_screen_enter = "on_screen_enter"_hs;
    constexpr auto on_collision_begin = "on_collision_begin"_hs;
    constexpr auto on_collision_end = "on_collision_end"_hs;
    constexpr auto on_animation_begin = "on_animation_begin"_hs;
    constexpr auto on_animation_end = "on_animation_end"_hs;
    constexpr auto on_press = "on_press"_hs;
    constexpr auto on_release = "on_release"_hs;
    constexpr auto on_hover = "on_hover"_hs;
    constexpr auto on_unhover = "on_unhover"_hs;
  }

  static void sync_body_position(body& bd, const transform& tf, const animation* an, bool changed) {
    const frame* frame = nullptr;
    if (an && an->playing && an->sheet->count > 0) [[likely]]
      frame = &an->sheet->frames[an->sheet->clips[an->active].offset + an->current];

    const auto center = center_of(bd, tf, frame);
    if (!changed) {
      const auto current = b2Body_GetPosition(bd.id);
      if (current.x == center.x && current.y == center.y)
        return;
    }

    b2Body_SetTransform(bd.id, center, b2Rot_identity);
  }

  entt::dense_map<entt::id_type, std::unique_ptr<prototype>> prototypes;

  static void commit(entt::registry& registry, entt::entity entity, scriptable& component) {
    auto* memory = static_cast<proxy*>(lua_newuserdata(L, sizeof(proxy)));
    luaL_getmetatable(L, "Object");
    lua_setmetatable(L, -2);
    component.handle = luaL_ref(L, LUA_REGISTRYINDEX);
    *memory = proxy{
      .registry = &registry,
      .entity = entity,
    };
  }

  static int index(lua_State* state) {
    const auto* self = static_cast<proxy*>(luaL_checkudata(state, 1, "Object"));
    auto length = 0uz;
    const auto* key = luaL_checklstring(state, 2, &length);
    const auto id = entt::hashed_string{key, length};
    const auto valid = self->registry && self->registry->valid(self->entity);

    if (id == lookup::alive) {
      lua_pushboolean(state, valid);
      return 1;
    }

    if (!valid) [[unlikely]]
      return lua_pushnil(state), 1;

    auto& registry = *self->registry;
    const auto entity = self->entity;

    switch (id) {
      case lookup::x:
        lua_pushnumber(state, static_cast<lua_Number>(registry.get<transform>(entity).x));
        return 1;

      case lookup::y:
        lua_pushnumber(state, static_cast<lua_Number>(registry.get<transform>(entity).y));
        return 1;

      case lookup::center_x: {
        const auto& tf = registry.get<transform>(entity);
        const auto* a = registry.try_get<animation>(entity);
        const auto* b = registry.try_get<body>(entity);
        const frame* frame = (a && a->playing && a->sheet->count > 0)
          ? &a->sheet->frames[a->sheet->clips[a->active].offset + a->current]
          : nullptr;
        const auto value = (b && frame) ? center_of(*b, tf, frame).x : tf.x;
        lua_pushnumber(state, static_cast<lua_Number>(value));
        return 1;
      }

      case lookup::center_y: {
        const auto& tf = registry.get<transform>(entity);
        const auto* a = registry.try_get<animation>(entity);
        const auto* b = registry.try_get<body>(entity);
        const frame* frame = (a && a->playing && a->sheet->count > 0)
          ? &a->sheet->frames[a->sheet->clips[a->active].offset + a->current]
          : nullptr;
        const auto value = (b && frame) ? center_of(*b, tf, frame).y : tf.y;
        lua_pushnumber(state, static_cast<lua_Number>(value));
        return 1;
      }

      case lookup::z:
        lua_pushinteger(state, static_cast<lua_Integer>(registry.get<renderable>(entity).z));
        return 1;

      case lookup::velocity_x: {
        const auto* b = registry.try_get<body>(entity);
        if (b && alive(*b)) [[likely]] {
          lua_pushnumber(state, static_cast<lua_Number>(b2Body_GetLinearVelocity(b->id).x));
          return 1;
        }
        lua_pushnumber(state, .0);
        return 1;
      }

      case lookup::velocity_y: {
        const auto* b = registry.try_get<body>(entity);
        if (b && alive(*b)) [[likely]] {
          lua_pushnumber(state, static_cast<lua_Number>(b2Body_GetLinearVelocity(b->id).y));
          return 1;
        }
        lua_pushnumber(state, .0);
        return 1;
      }

      case lookup::flip:
        lua_pushinteger(state, static_cast<lua_Integer>(registry.get<transform>(entity).flip));
        return 1;

      case lookup::dormant:
        lua_pushboolean(state, registry.all_of<dormant>(entity));
        return 1;

      case lookup::animation: {
        const auto* a = registry.try_get<animation>(entity);
        if (!a || !a->playing || a->sheet->count == 0) [[unlikely]]
          return lua_pushnil(state), 1;

        lua_rawgeti(state, LUA_REGISTRYINDEX, a->sheet->clips[a->active].identity.name);
        return 1;
      }

      case lookup::shown:
        lua_pushboolean(state, registry.get<transform>(entity).shown);
        return 1;

      case lookup::scale:
        lua_pushnumber(state, static_cast<lua_Number>(registry.get<transform>(entity).scale));
        return 1;

      case lookup::angle:
        lua_pushnumber(state, static_cast<lua_Number>(registry.get<transform>(entity).angle));
        return 1;

      case lookup::alpha:
        lua_pushnumber(state, static_cast<lua_Number>(registry.get<transform>(entity).alpha));
        return 1;

      case lookup::name:
        lua_rawgeti(state, LUA_REGISTRYINDEX, registry.get<scriptable>(entity).label);
        return 1;

      case lookup::kind:
        lua_rawgeti(state, LUA_REGISTRYINDEX, registry.get<scriptable>(entity).blueprint->kind);
        return 1;

      case lookup::body:
      case lookup::sleepable:
      case lookup::on_spawn:
      case lookup::on_loop:
      case lookup::on_sleep:
      case lookup::on_wake:
      case lookup::on_screen_exit:
      case lookup::on_screen_enter:
      case lookup::on_collision_begin:
      case lookup::on_collision_end:
      case lookup::on_animation_begin:
      case lookup::on_animation_end:
      case lookup::on_press:
      case lookup::on_release:
      case lookup::on_hover:
      case lookup::on_unhover:
        return lua_pushnil(state), 1;

      default: {
        const auto& op = registry.get<scriptable>(entity);
        assert(op.blueprint && op.blueprint->table != LUA_NOREF && "object must have an object reference");

        lua_rawgeti(state, LUA_REGISTRYINDEX, op.blueprint->table);
        lua_getfield(state, -1, key);
        if (!lua_isnil(state, -1)) [[likely]] {
          lua_remove(state, -2);
          return 1;
        }
        lua_pop(state, 1);

        constexpr auto prefix = 3uz;
        constexpr auto limit = 60uz;
        assert(length <= limit &&
               "key is too long and would be truncated to 60 characters.");

        std::array<char, prefix + limit + 1uz> buffer;
        const auto size = std::min(length, limit);
        buffer[0] = 'o';
        buffer[1] = 'n';
        buffer[2] = '_';
        std::memcpy(buffer.data() + prefix, key, size);
        buffer[prefix + size] = '\0';

        switch (entt::hashed_string{buffer.data()}) {
          case lookup::on_spawn:
          case lookup::on_loop:
          case lookup::on_sleep:
          case lookup::on_wake:
          case lookup::on_screen_exit:
          case lookup::on_screen_enter:
          case lookup::on_collision_begin:
          case lookup::on_collision_end:
          case lookup::on_animation_begin:
          case lookup::on_animation_end:
          case lookup::on_press:
          case lookup::on_release:
          case lookup::on_hover:
          case lookup::on_unhover:
            lua_pop(state, 1);
            return lua_pushnil(state), 1;

          default:
            break;
        }

        lua_getfield(state, -1, buffer.data());
        lua_remove(state, -2);
        if (!lua_isnil(state, -1))
          return 1;
        lua_pop(state, 1);

        return lua_pushnil(state), 1;
      }
    }
  }

  static int newindex(lua_State* state) {
    auto* self = static_cast<proxy*>(luaL_checkudata(state, 1, "Object"));
    const auto* key = luaL_checkstring(state, 2);
    const auto id = entt::hashed_string{key};

    if (!self->registry || !self->registry->valid(self->entity)) [[unlikely]]
      return 0;

    auto& registry = *self->registry;
    const auto entity = self->entity;

    switch (id) {
      case lookup::x: {
        auto& tf = registry.get<transform>(entity);
        const auto value = static_cast<float>(luaL_checknumber(state, 3));
        const auto changed = tf.x != value;
        tf.previous_x = tf.x = value;
        if (changed && registry.all_of<dormant>(entity))
          registry.ctx().get<dormancy>().dirty = true;

        auto* b = registry.try_get<body>(entity);
        if (b && anchored(*b)) [[likely]]
          sync_body_position(*b, tf, registry.try_get<animation>(entity), changed);

        return 0;
      }

      case lookup::y: {
        auto& tf = registry.get<transform>(entity);
        const auto value = static_cast<float>(luaL_checknumber(state, 3));
        const auto changed = tf.y != value;
        tf.previous_y = tf.y = value;
        if (changed && registry.all_of<dormant>(entity))
          registry.ctx().get<dormancy>().dirty = true;

        auto* b = registry.try_get<body>(entity);
        if (b && anchored(*b)) [[likely]]
          sync_body_position(*b, tf, registry.try_get<animation>(entity), changed);

        return 0;
      }

      case lookup::z: {
        auto& r = registry.get<renderable>(entity);
        const auto value = static_cast<int>(luaL_checkinteger(state, 3));
        if (r.z == value)
          return 0;

        r.z = value;
        registry.ctx().get<reorder>().dirty = true;

        return 0;
      }

      case lookup::velocity_x: {
        auto* b = registry.try_get<body>(entity);
        if (b && propelled(*b)) [[likely]] {
          const auto value = static_cast<float>(luaL_checknumber(state, 3));
          const auto current = b2Body_GetLinearVelocity(b->id);
          if (current.x != value ||
              ((value != .0f || current.y != .0f) && !b2Body_IsAwake(b->id)))
            b2Body_SetLinearVelocity(b->id, {value, current.y});
        }

        return 0;
      }

      case lookup::velocity_y: {
        auto* b = registry.try_get<body>(entity);
        if (b && propelled(*b)) [[likely]] {
          const auto value = static_cast<float>(luaL_checknumber(state, 3));
          const auto current = b2Body_GetLinearVelocity(b->id);
          if (current.y != value ||
              ((current.x != .0f || value != .0f) && !b2Body_IsAwake(b->id)))
            b2Body_SetLinearVelocity(b->id, {current.x, value});
        }

        return 0;
      }

      case lookup::flip: {
        const auto value = std::clamp(luaL_checkinteger(state, 3), lua_Integer{}, lua_Integer{3});
        registry.get<transform>(entity).flip = static_cast<mirror>(value);
        return 0;
      }

      case lookup::animation: {
        auto* a = registry.try_get<animation>(entity);
        assert(a != nullptr && "object must have animation");
        [[assume(a != nullptr)]];

        const auto hash = entt::hashed_string{luaL_checkstring(state, 3)};
        if (a->playing && a->sheet->clips[a->active].identity.hash == hash)
          return 0;

        for (uint8_t i = 0; i < a->sheet->count; ++i) {
          if (a->sheet->clips[i].identity.hash != hash)
            continue;

          const auto& clip = a->sheet->clips[a->active];
          const auto callback = a->playing ? clip.identity.name : LUA_NOREF;
          const auto identity = clip.identity.hash;
          a->active = i;
          a->current = 0;
          a->elapsed = .0f;
          a->playing = true;
          if (registry.all_of<dormant>(entity))
            registry.ctx().get<dormancy>().dirty = true;

          if (a->sheet->clips[i].effect)
            a->sheet->clips[i].effect->play();

          const auto& op = registry.get<scriptable>(entity);
          if (op.handle == LUA_NOREF)
            return 0;

          const auto handle = op.handle;
          const auto ending = op.blueprint->on_animation_end;
          const auto beginning = op.blueprint->on_animation_begin;

          if (callback != LUA_NOREF && identity != hash && ending != LUA_NOREF) [[unlikely]] {
            lua_rawgeti(state, LUA_REGISTRYINDEX, ending);
            lua_rawgeti(state, LUA_REGISTRYINDEX, handle);
            lua_rawgeti(state, LUA_REGISTRYINDEX, callback);
            lua_call(state, 2, 0);
          }

          if (!registry.valid(entity)) return 0;

          if (beginning != LUA_NOREF) {
            lua_rawgeti(state, LUA_REGISTRYINDEX, beginning);
            lua_rawgeti(state, LUA_REGISTRYINDEX, handle);
            lua_rawgeti(state, LUA_REGISTRYINDEX, a->sheet->clips[i].identity.name);
            lua_call(state, 2, 0);
          }

          return 0;
        }

        return 0;
      }

      case lookup::scale: {
        auto& tf = registry.get<transform>(entity);
        const auto value = static_cast<float>(luaL_checknumber(state, 3));
        if (tf.scale != value && registry.all_of<dormant>(entity))
          registry.ctx().get<dormancy>().dirty = true;
        tf.scale = value;
        return 0;
      }

      case lookup::angle:
        registry.get<transform>(entity).angle = static_cast<float>(luaL_checknumber(state, 3));
        return 0;

      case lookup::alpha:
        registry.get<transform>(entity).alpha = std::clamp(
          static_cast<float>(luaL_checknumber(state, 3)), .0f, 255.f);
        return 0;

      case lookup::shown:
        registry.get<transform>(entity).shown = lua_toboolean(state, 3) != 0;
        return 0;

      case lookup::center_x:
      case lookup::center_y:
      case lookup::name:
      case lookup::kind:
      case lookup::alive:
      case lookup::dormant:
      case lookup::body:
      case lookup::sleepable:
      case lookup::on_spawn:
      case lookup::on_loop:
      case lookup::on_sleep:
      case lookup::on_wake:
      case lookup::on_screen_exit:
      case lookup::on_screen_enter:
      case lookup::on_collision_begin:
      case lookup::on_collision_end:
      case lookup::on_animation_begin:
      case lookup::on_animation_end:
      case lookup::on_press:
      case lookup::on_release:
      case lookup::on_hover:
      case lookup::on_unhover:
        return 0;

      default: {
        const auto& op = registry.get<scriptable>(entity);
        assert(op.blueprint && op.blueprint->table != LUA_NOREF && "object must have an object reference");

        lua_rawgeti(state, LUA_REGISTRYINDEX, op.blueprint->table);
        lua_pushvalue(state, 3);
        lua_setfield(state, -2, key);
        lua_pop(state, 1);
        return 0;
      }
    }
  }
}

void object::bind(entt::registry& registry, entt::entity entity, scriptable& component, std::string_view name, std::string_view kind) {
  lua_pushlstring(L, name.data(), name.size());
  component.label = luaL_ref(L, LUA_REGISTRYINDEX);

  const auto identity = entt::hashed_string{kind.data(), kind.size()};

  if (const auto it = prototypes.find(identity); it != prototypes.end()) [[likely]] {
    component.blueprint = it->second.get();
    return commit(registry, entity, component);
  }

  depot->source.insert(kind);
  {
    const auto base = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
    lua_insert(L, base);
    const auto status = lua_pcall(L, 0, 1, base);
    lua_remove(L, base);
    if (status != LUA_OK) [[unlikely]] {
      lua_error(L);
      std::unreachable();
    }
  }

  auto blueprint = std::make_unique<prototype>();
  blueprint->table = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_pushlstring(L, kind.data(), kind.size());
  blueprint->kind = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, blueprint->table);

  constexpr std::array fields{
    std::pair{"on_loop", &prototype::on_loop},
    std::pair{"on_animation_end", &prototype::on_animation_end},
    std::pair{"on_animation_begin", &prototype::on_animation_begin},
    std::pair{"on_collision_begin", &prototype::on_collision_begin},
    std::pair{"on_collision_end", &prototype::on_collision_end},
    std::pair{"on_wake", &prototype::on_wake},
    std::pair{"on_sleep", &prototype::on_sleep},
    std::pair{"on_screen_exit", &prototype::on_screen_exit},
    std::pair{"on_screen_enter", &prototype::on_screen_enter},
    std::pair{"on_spawn", &prototype::on_spawn},
    std::pair{"on_press", &prototype::on_press},
    std::pair{"on_release", &prototype::on_release},
    std::pair{"on_hover", &prototype::on_hover},
    std::pair{"on_unhover", &prototype::on_unhover},
  };

  for (const auto& [name, member] : fields) {
    lua_getfield(L, -1, name);

    blueprint.get()->*member = lua_isfunction(L, -1)
      ? luaL_ref(L, LUA_REGISTRYINDEX)
      : (lua_pop(L, 1), LUA_NOREF);
  }

  lua_pop(L, 1);

  component.blueprint = blueprint.get();
  prototypes.emplace(identity, std::move(blueprint));
  commit(registry, entity, component);
}

void object::wire() {
  luaL_newmetatable(L, "Object");
  lua_pushliteral(L, "Object");
  lua_setfield(L, -2, "__name");

  lua_pushcfunction(L, index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, newindex);
  lua_setfield(L, -2, "__newindex");
  lua_pop(L, 1);

  lua_createtable(L, 0, 4);
  lua_pushinteger(L, static_cast<lua_Integer>(mirror::none));
  lua_setfield(L, -2, "none");
  lua_pushinteger(L, static_cast<lua_Integer>(mirror::horizontal));
  lua_setfield(L, -2, "horizontal");
  lua_pushinteger(L, static_cast<lua_Integer>(mirror::vertical));
  lua_setfield(L, -2, "vertical");
  lua_pushinteger(L, static_cast<lua_Integer>(mirror::both));
  lua_setfield(L, -2, "both");
  lua_setglobal(L, "flip");
}
