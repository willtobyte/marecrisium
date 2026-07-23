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
  }

  static void sync_body_position(body& bd, const transform& tf, const animation* an) {
    const frame* fr = nullptr;
    if (an && an->playing && an->sheet->count > 0) [[likely]]
      fr = &an->sheet->frames[an->sheet->clips[an->active].offset + an->current];

    b2Body_SetTransform(bd.id, center_of(bd, tf, fr), b2Rot_identity);
  }

  struct prototype {
    int reference{LUA_NOREF};
    int on_loop{LUA_NOREF};
    int on_animation_end{LUA_NOREF};
    int on_animation_begin{LUA_NOREF};
    int on_collision_begin{LUA_NOREF};
    int on_collision_end{LUA_NOREF};
    int on_wake{LUA_NOREF};
    int on_sleep{LUA_NOREF};
    int on_screen_exit{LUA_NOREF};
    int on_screen_enter{LUA_NOREF};
    int on_spawn{LUA_NOREF};
    int on_press{LUA_NOREF};
    int on_release{LUA_NOREF};
    int on_hover{LUA_NOREF};
    int on_unhover{LUA_NOREF};
  };

  ankerl::unordered_dense::map<entt::id_type, prototype> prototypes;

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

  static int object_index(lua_State* state) {
    const auto* self = static_cast<proxy*>(luaL_checkudata(state, 1, "Object"));
    const auto* key = luaL_checkstring(state, 2);
    const auto id = entt::hashed_string{key};

    if (id == lookup::alive) {
      lua_pushboolean(state, self->registry->valid(self->entity) ? 1 : 0);
      return 1;
    }

    if (!self->registry->valid(self->entity)) [[unlikely]]
      return lua_pushnil(state), 1;

    auto& registry = *self->registry;
    const auto entity = self->entity;
    const auto& op = registry.get<scriptable>(entity);

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
        const frame* fr = (a && a->playing && a->sheet->count > 0)
          ? &a->sheet->frames[a->sheet->clips[a->active].offset + a->current]
          : nullptr;
        const auto value = (b && fr) ? center_of(*b, tf, fr).x : tf.x;
        lua_pushnumber(state, static_cast<lua_Number>(value));
        return 1;
      }

      case lookup::center_y: {
        const auto& tf = registry.get<transform>(entity);
        const auto* a = registry.try_get<animation>(entity);
        const auto* b = registry.try_get<body>(entity);
        const frame* fr = (a && a->playing && a->sheet->count > 0)
          ? &a->sheet->frames[a->sheet->clips[a->active].offset + a->current]
          : nullptr;
        const auto value = (b && fr) ? center_of(*b, tf, fr).y : tf.y;
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
        lua_pushboolean(state, registry.all_of<dormant>(entity) ? 1 : 0);
        return 1;

      case lookup::animation: {
        const auto* a = registry.try_get<animation>(entity);
        if (!a || !a->playing || a->sheet->count == 0) [[unlikely]]
          return lua_pushnil(state), 1;

        lua_rawgeti(state, LUA_REGISTRYINDEX, a->sheet->clips[a->active].identity.reference);
        return 1;
      }

      case lookup::shown:
        lua_pushboolean(state, registry.get<transform>(entity).shown ? 1 : 0);
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
        lua_rawgeti(state, LUA_REGISTRYINDEX, op.name_reference);
        return 1;

      case lookup::kind:
        lua_rawgeti(state, LUA_REGISTRYINDEX, op.kind_reference);
        return 1;

      default: {
        assert(op.prototype != LUA_NOREF && "object must have an object reference");

        lua_rawgeti(state, LUA_REGISTRYINDEX, op.prototype);
        lua_getfield(state, -1, key);
        if (!lua_isnil(state, -1)) [[likely]] {
          lua_remove(state, -2);
          return 1;
        }
        lua_pop(state, 1);

        assert(std::strlen(key) <= 60 &&
               "key is too long and would be truncated to 60 characters.");

        static std::array<char, 64> buffer;
        const auto length = std::min(std::strlen(key), std::size_t{60});
        buffer[0] = 'o';
        buffer[1] = 'n';
        buffer[2] = '_';
        std::memcpy(buffer.data() + 3, key, length);
        buffer[3 + length] = '\0';

        lua_getfield(state, -1, buffer.data());
        lua_remove(state, -2);
        if (!lua_isnil(state, -1))
          return 1;
        lua_pop(state, 1);

        return lua_pushnil(state), 1;
      }
    }
  }

  static int object_newindex(lua_State* state) {
    auto* self = static_cast<proxy*>(luaL_checkudata(state, 1, "Object"));
    const auto* key = luaL_checkstring(state, 2);
    const auto id = entt::hashed_string{key};

    if (!self->registry->valid(self->entity))
      return 0;

    auto& registry = *self->registry;
    const auto entity = self->entity;
    const auto& op = registry.get<scriptable>(entity);

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
          sync_body_position(*b, tf, registry.try_get<animation>(entity));

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
          sync_body_position(*b, tf, registry.try_get<animation>(entity));

        return 0;
      }

      case lookup::z: {
        auto& r = registry.get<renderable>(entity);
        const auto value = static_cast<int>(luaL_checkinteger(state, 3));
        r.z = value;
        registry.ctx().get<reorder>().dirty = true;

        return 0;
      }

      case lookup::velocity_x: {
        auto* b = registry.try_get<body>(entity);
        if (b && propelled(*b)) [[likely]] {
          const auto current = b2Body_GetLinearVelocity(b->id);
          b2Body_SetLinearVelocity(b->id, {static_cast<float>(luaL_checknumber(state, 3)), current.y});
        }

        return 0;
      }

      case lookup::velocity_y: {
        auto* b = registry.try_get<body>(entity);
        if (b && propelled(*b)) [[likely]] {
          const auto current = b2Body_GetLinearVelocity(b->id);
          b2Body_SetLinearVelocity(b->id, {current.x, static_cast<float>(luaL_checknumber(state, 3))});
        }

        return 0;
      }

      case lookup::flip: {
        const auto value = static_cast<uint8_t>(luaL_checkinteger(state, 3));
        if (value > 3) [[unlikely]]
          return luaL_error(state, "invalid flip value: %d", value);
        registry.get<transform>(entity).flip = static_cast<mirror>(value);
        return 0;
      }

      case lookup::animation: {
        auto* a = registry.try_get<animation>(entity);
        if (!a) [[unlikely]]
          return 0;

        const auto hash = entt::hashed_string{luaL_checkstring(state, 3)};
        for (uint8_t i = 0; i < a->sheet->count; ++i) {
          if (a->sheet->clips[i].identity.hash != hash)
            continue;

          if (a->active == i && a->playing)
            return 0;

          const auto& pc = a->sheet->clips[a->active];
          const auto callback = a->playing ? pc.identity.reference : LUA_NOREF;
          const auto identity = pc.identity.hash;
          a->active = i;
          a->current = 0;
          a->elapsed = .0f;
          a->playing = true;
          if (registry.all_of<dormant>(entity))
            registry.ctx().get<dormancy>().dirty = true;

          if (a->sheet->clips[i].effect)
            a->sheet->clips[i].effect->play();

          if (op.handle == LUA_NOREF)
            return 0;

          if (callback != LUA_NOREF && identity != hash && op.on_animation_end != LUA_NOREF) [[unlikely]] {
            lua_rawgeti(state, LUA_REGISTRYINDEX, op.on_animation_end);
            lua_rawgeti(state, LUA_REGISTRYINDEX, op.handle);
            lua_rawgeti(state, LUA_REGISTRYINDEX, callback);
            binding::call(state, 2, 0);
          }

          if (!registry.valid(entity)) return 0;

          if (op.on_animation_begin != LUA_NOREF) {
            lua_rawgeti(state, LUA_REGISTRYINDEX, op.on_animation_begin);
            lua_rawgeti(state, LUA_REGISTRYINDEX, op.handle);
            lua_rawgeti(state, LUA_REGISTRYINDEX, a->sheet->clips[i].identity.reference);
            binding::call(state, 2, 0);
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
        registry.get<transform>(entity).alpha =
          std::clamp(static_cast<float>(luaL_checknumber(state, 3)), .0f, 255.0f);
        return 0;

      case lookup::shown:
        registry.get<transform>(entity).shown = lua_toboolean(state, 3) != 0;
        return 0;

      default:
        assert(op.prototype != LUA_NOREF && "object must have an object reference");

        lua_rawgeti(state, LUA_REGISTRYINDEX, op.prototype);
        lua_pushvalue(state, 3);
        lua_setfield(state, -2, key);
        lua_pop(state, 1);
        return 0;
    }
  }
}


void object::bind(entt::registry& registry, entt::entity entity, scriptable& component, std::string_view name, std::string_view kind) {
  depot->source.insert(kind);

  lua_pushlstring(L, name.data(), name.size());
  component.name_reference = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_pushlstring(L, kind.data(), kind.size());
  component.kind_reference = luaL_ref(L, LUA_REGISTRYINDEX);

  const auto identity = entt::hashed_string{kind.data(), kind.size()};

  if (const auto it = prototypes.find(identity); it != prototypes.end()) [[likely]] {
    lua_pop(L, 1);

    const auto& blueprint = it->second;
    component.prototype = blueprint.reference;
    component.on_loop = blueprint.on_loop;
    component.on_animation_end = blueprint.on_animation_end;
    component.on_animation_begin = blueprint.on_animation_begin;
    component.on_collision_begin = blueprint.on_collision_begin;
    component.on_collision_end = blueprint.on_collision_end;
    component.on_wake = blueprint.on_wake;
    component.on_sleep = blueprint.on_sleep;
    component.on_screen_exit = blueprint.on_screen_exit;
    component.on_screen_enter = blueprint.on_screen_enter;
    component.on_spawn = blueprint.on_spawn;
    component.on_press = blueprint.on_press;
    component.on_release = blueprint.on_release;
    component.on_hover = blueprint.on_hover;
    component.on_unhover = blueprint.on_unhover;

    return commit(registry, entity, component);
  }

  binding::call(L, 0, 1);

  component.prototype = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, component.prototype);

  lua_getfield(L, -1, "on_loop");
  component.on_loop = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_animation_end");
  component.on_animation_end = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_animation_begin");
  component.on_animation_begin = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_collision_begin");
  component.on_collision_begin = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_collision_end");
  component.on_collision_end = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_wake");
  component.on_wake = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_sleep");
  component.on_sleep = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_screen_exit");
  component.on_screen_exit = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_screen_enter");
  component.on_screen_enter = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_spawn");
  component.on_spawn = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_press");
  component.on_press = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_release");
  component.on_release = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_hover");
  component.on_hover = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_unhover");
  component.on_unhover = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_pop(L, 1);

  prototypes.emplace(identity, prototype{
    component.prototype,
    component.on_loop,
    component.on_animation_end,
    component.on_animation_begin,
    component.on_collision_begin,
    component.on_collision_end,
    component.on_wake,
    component.on_sleep,
    component.on_screen_exit,
    component.on_screen_enter,
    component.on_spawn,
    component.on_press,
    component.on_release,
    component.on_hover,
    component.on_unhover,
  });

  commit(registry, entity, component);
}

void object::wire() {
  binding::metatable(L, "Object", object_index, object_newindex);

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
