namespace {
  namespace lookup {
    constexpr auto alive = "alive"_hs;
    constexpr auto x = "x"_hs;
    constexpr auto y = "y"_hs;
    constexpr auto z = "z"_hs;
    constexpr auto flip = "flip"_hs;
    constexpr auto animation = "animation"_hs;
    constexpr auto shown = "shown"_hs;
    constexpr auto scale = "scale"_hs;
    constexpr auto angle = "angle"_hs;
    constexpr auto alpha = "alpha"_hs;
    constexpr auto name = "name"_hs;
    constexpr auto kind = "kind"_hs;
    constexpr auto on_spawn = "on_spawn"_hs;
    constexpr auto on_loop = "on_loop"_hs;
    constexpr auto on_animation_begin = "on_animation_begin"_hs;
    constexpr auto on_animation_end = "on_animation_end"_hs;
    constexpr auto on_press = "on_press"_hs;
    constexpr auto on_release = "on_release"_hs;
    constexpr auto on_hover = "on_hover"_hs;
    constexpr auto on_unhover = "on_unhover"_hs;
  }

  struct prototype_hash final {
    using is_transparent = void;

    std::size_t operator()(std::string_view value) const noexcept {
      return entt::hashed_string{value.data(), value.size()}.value();
    }
  };

  entt::dense_map<std::string, std::unique_ptr<prototype>, prototype_hash, std::equal_to<>> prototypes;

  static void commit(entt::registry& registry, entt::entity entity, scriptable& component) {
    auto* memory = static_cast<proxy*>(lua_newuserdata(L, sizeof(proxy)));
    luaL_getmetatable(L, "Object");
    lua_setmetatable(L, -2);
    lua_newtable(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, component.blueprint->table);
    lua_setmetatable(L, -2);
    lua_setfenv(L, -2);
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

      case lookup::z:
        lua_pushinteger(state, static_cast<lua_Integer>(registry.get<renderable>(entity).z));
        return 1;

      case lookup::flip:
        lua_pushinteger(state, static_cast<lua_Integer>(registry.get<transform>(entity).flip));
        return 1;

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

      case lookup::animation:
      case lookup::on_spawn:
      case lookup::on_loop:
      case lookup::on_animation_begin:
      case lookup::on_animation_end:
      case lookup::on_press:
      case lookup::on_release:
      case lookup::on_hover:
      case lookup::on_unhover:
        return lua_pushnil(state), 1;

      default: {
        lua_getfenv(state, 1);
        lua_pushvalue(state, 2);
        lua_gettable(state, -2);
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
        tf.x = value;
        return 0;
      }

      case lookup::y: {
        auto& tf = registry.get<transform>(entity);
        const auto value = static_cast<float>(luaL_checknumber(state, 3));
        tf.y = value;
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

      case lookup::flip: {
        const auto value = std::clamp(luaL_checkinteger(state, 3), lua_Integer{}, lua_Integer{3});
        registry.get<transform>(entity).flip = static_cast<mirror>(value);
        return 0;
      }

      case lookup::scale: {
        auto& tf = registry.get<transform>(entity);
        const auto value = static_cast<float>(luaL_checknumber(state, 3));
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

      case lookup::shown: {
        auto& tf = registry.get<transform>(entity);
        const auto value = lua_toboolean(state, 3) != 0;
        if (tf.shown == value)
          return 0;

        tf.shown = value;
        return 0;
      }

      case lookup::name:
      case lookup::kind:
      case lookup::alive:
      case lookup::animation:
      case lookup::on_spawn:
      case lookup::on_loop:
      case lookup::on_animation_begin:
      case lookup::on_animation_end:
      case lookup::on_press:
      case lookup::on_release:
      case lookup::on_hover:
      case lookup::on_unhover:
        return 0;

      default:
        lua_getfenv(state, 1);
        lua_pushvalue(state, 2);
        lua_pushvalue(state, 3);
        lua_rawset(state, -3);
        lua_pop(state, 1);
        return 0;
    }
  }
}

void object::bind(entt::registry& registry, entt::entity entity, scriptable& component, std::string_view name, std::string_view kind) {
  lua_pushlstring(L, name.data(), name.size());
  component.label = luaL_ref(L, LUA_REGISTRYINDEX);

  if (const auto it = prototypes.find(kind); it != prototypes.end()) [[likely]] {
    component.blueprint = it->second.get();
    return commit(registry, entity, component);
  }

  const auto chunk = std::format("@objects/{}.lua", kind);
  const auto path = std::string_view{chunk}.substr(1);
  const auto source = io::read(path);
  if (luaL_loadbuffer(L, reinterpret_cast<const char*>(source.data()), source.size(), chunk.c_str()) != LUA_OK) [[unlikely]]
    lua_error(L);

  if (lua_pcall(L, 0, 1, 0) != LUA_OK) [[unlikely]]
    lua_error(L);

  auto blueprint = std::make_unique<prototype>();
  blueprint->table = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_pushlstring(L, kind.data(), kind.size());
  blueprint->kind = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, blueprint->table);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");

  constexpr std::array fields{
    std::pair{"on_loop", &prototype::on_loop},
    std::pair{"on_animation_end", &prototype::on_animation_end},
    std::pair{"on_animation_begin", &prototype::on_animation_begin},
    std::pair{"on_spawn", &prototype::on_spawn},
    std::pair{"on_press", &prototype::on_press},
    std::pair{"on_release", &prototype::on_release},
    std::pair{"on_hover", &prototype::on_hover},
    std::pair{"on_unhover", &prototype::on_unhover},
  };

  for (const auto& [field, member] : fields) {
    lua_getfield(L, -1, field);

    blueprint.get()->*member = lua_isfunction(L, -1)
      ? luaL_ref(L, LUA_REGISTRYINDEX)
      : (lua_pop(L, 1), LUA_NOREF);
  }

  lua_pop(L, 1);

  component.blueprint = blueprint.get();
  prototypes.emplace(kind, std::move(blueprint));
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
