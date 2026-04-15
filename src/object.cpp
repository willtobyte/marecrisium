#include "object.hpp"
#include "sound.hpp"

namespace {
  namespace property {
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
  }

  static void sync_body_position(body& bd, const transform& tf, const animation* an) noexcept {
    auto ox = .0f;
    auto oy = .0f;
    if (an && an->playing && an->sheet->count > 0) [[likely]] {
      const auto& frame = an->sheet->frames[an->sheet->clips[an->active].offset + an->current];
      ox = frame.bound_x;
      oy = frame.bound_y;
    }

    b2Body_SetTransform(bd.id, {tf.x + ox + bd.extent_x, tf.y + oy + bd.extent_y}, b2Rot_identity);
  }

  int object_index(lua_State* state) {
    const auto* proxy = static_cast<scriptable*>(lua_touserdata(state, 1));
    const auto* key = luaL_checkstring(state, 2);
    const auto id = entt::hashed_string{key};

    if (id == property::alive) {
      lua_pushboolean(state, proxy->registry->valid(proxy->entity) ? 1 : 0);
      return 1;
    }

    if (!proxy->registry->valid(proxy->entity)) [[unlikely]]
      return lua_pushnil(state), 1;

    auto& registry = *proxy->registry;
    const auto entity = proxy->entity;

    switch (id) {
      case property::x:
        lua_pushnumber(state, static_cast<lua_Number>(registry.get<transform>(entity).x));
        return 1;

      case property::y:
        lua_pushnumber(state, static_cast<lua_Number>(registry.get<transform>(entity).y));
        return 1;

      case property::z:
        lua_pushinteger(state, static_cast<lua_Integer>(registry.get<renderable>(entity).z));
        return 1;

      case property::velocity_x: {
        const auto* b = registry.try_get<body>(entity);
        if (b && b2Body_IsValid(b->id)) [[likely]] {
          lua_pushnumber(state, static_cast<lua_Number>(b2Body_GetLinearVelocity(b->id).x));
          return 1;
        }
        lua_pushnumber(state, .0);
        return 1;
      }

      case property::velocity_y: {
        const auto* b = registry.try_get<body>(entity);
        if (b && b2Body_IsValid(b->id)) [[likely]] {
          lua_pushnumber(state, static_cast<lua_Number>(b2Body_GetLinearVelocity(b->id).y));
          return 1;
        }
        lua_pushnumber(state, .0);
        return 1;
      }

      case property::flip:
        lua_pushinteger(state, static_cast<lua_Integer>(registry.get<transform>(entity).flip));
        return 1;

      case property::dormant:
        lua_pushboolean(state, registry.all_of<dormant>(entity) ? 1 : 0);
        return 1;

      case property::animation: {
        if (!registry.all_of<animation>(entity))
          return lua_pushnil(state), 1;

        const auto& a = registry.get<animation>(entity);
        if (!a.playing || a.sheet->count == 0) [[unlikely]]
          return lua_pushnil(state), 1;

        lua_rawgeti(state, LUA_REGISTRYINDEX, a.sheet->clips[a.active].label);
        return 1;
      }

      case property::shown:
        lua_pushboolean(state, registry.get<transform>(entity).shown ? 1 : 0);
        return 1;

      case property::scale:
        lua_pushnumber(state, static_cast<lua_Number>(registry.get<transform>(entity).scale));
        return 1;

      case property::angle:
        lua_pushnumber(state, static_cast<lua_Number>(registry.get<transform>(entity).angle));
        return 1;

      case property::alpha:
        lua_pushnumber(state, static_cast<lua_Number>(registry.get<transform>(entity).alpha));
        return 1;

      case property::name:
        lua_rawgeti(state, LUA_REGISTRYINDEX, proxy->name_ref);
        return 1;

      case property::kind:
        lua_rawgeti(state, LUA_REGISTRYINDEX, proxy->kind_ref);
        return 1;

      default: {
        assert(proxy->prototype != LUA_NOREF && "object must have an object reference");

        lua_rawgeti(state, LUA_REGISTRYINDEX, proxy->prototype);
        lua_getfield(state, -1, key);
        if (!lua_isnil(state, -1)) [[likely]] {
          lua_remove(state, -2);
          return 1;
        }
        lua_pop(state, 1);

        std::array<char, 64> buffer;
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

  int object_newindex(lua_State* state) {
    auto* proxy = static_cast<scriptable*>(lua_touserdata(state, 1));
    const auto* key = luaL_checkstring(state, 2);
    const auto id = entt::hashed_string{key};

    if (!proxy->registry->valid(proxy->entity))
      return 0;

    auto& registry = *proxy->registry;
    const auto entity = proxy->entity;

    switch (id) {
      case property::x: {
        auto& tf = registry.get<transform>(entity);
        tf.previous_x = tf.x = static_cast<float>(luaL_checknumber(state, 3));

        auto* b = registry.try_get<body>(entity);
        if (b
            && b->type != body_type::kinematic
            && b2Body_IsValid(b->id)) [[likely]]
          sync_body_position(*b, tf, registry.try_get<animation>(entity));

        return 0;
      }

      case property::y: {
        auto& tf = registry.get<transform>(entity);
        tf.previous_y = tf.y = static_cast<float>(luaL_checknumber(state, 3));

        auto* b = registry.try_get<body>(entity);
        if (b
            && b->type != body_type::kinematic
            && b2Body_IsValid(b->id)) [[likely]]
          sync_body_position(*b, tf, registry.try_get<animation>(entity));

        return 0;
      }

      case property::z: {
        auto& r = registry.get<renderable>(entity);
        const auto value = static_cast<int>(luaL_checkinteger(state, 3));
        r.z = value;
        registry.ctx().get<reorder>().dirty = true;

        return 0;
      }

      case property::velocity_x: {
        auto* b = registry.try_get<body>(entity);
        if (b
            && b->type == body_type::dynamic
            && b2Body_IsValid(b->id)) [[likely]] {
          const auto current = b2Body_GetLinearVelocity(b->id);
          b2Body_SetLinearVelocity(b->id, {static_cast<float>(luaL_checknumber(state, 3)), current.y});
        }

        return 0;
      }

      case property::velocity_y: {
        auto* b = registry.try_get<body>(entity);
        if (b
            && b->type == body_type::dynamic
            && b2Body_IsValid(b->id)) [[likely]] {
          const auto current = b2Body_GetLinearVelocity(b->id);
          b2Body_SetLinearVelocity(b->id, {current.x, static_cast<float>(luaL_checknumber(state, 3))});
        }

        return 0;
      }

      case property::flip: {
        const auto value = static_cast<uint8_t>(luaL_checkinteger(state, 3));
        if (value > 3) [[unlikely]]
          return luaL_error(state, "invalid flip value: %d", value);
        registry.get<transform>(entity).flip = static_cast<mirror>(value);
        return 0;
      }

      case property::animation: {
        if (!registry.all_of<animation>(entity))
          return 0;

        const std::string_view value = luaL_checkstring(state, 3);
        const auto hash = entt::hashed_string{value.data()};

        auto& a = registry.get<animation>(entity);

        for (uint8_t i = 0; i < a.sheet->count; ++i) {
          if (a.sheet->clips[i].name != hash)
            continue;

          const auto& pc = a.sheet->clips[a.active];
          const auto previous = a.playing ? pc.label : LUA_NOREF;
          a.active = i;
          a.current = 0;
          a.elapsed = .0f;
          a.playing = true;

          if (a.sheet->clips[i].effect)
            a.sheet->clips[i].effect->play();

          if (proxy->handle == LUA_NOREF)
            return 0;

          if (previous != LUA_NOREF
              && pc.name != hash
              && proxy->on_animation_end != LUA_NOREF) [[unlikely]] {
            lua_rawgeti(state, LUA_REGISTRYINDEX, proxy->on_animation_end);
            lua_rawgeti(state, LUA_REGISTRYINDEX, proxy->handle);
            lua_rawgeti(state, LUA_REGISTRYINDEX, previous);
            pcall(state, 2, 0);
          }

          if (proxy->on_animation_begin != LUA_NOREF) {
            lua_rawgeti(state, LUA_REGISTRYINDEX, proxy->on_animation_begin);
            lua_rawgeti(state, LUA_REGISTRYINDEX, proxy->handle);
            lua_rawgeti(state, LUA_REGISTRYINDEX, a.sheet->clips[i].label);
            pcall(state, 2, 0);
          }

          return 0;
        }

        return 0;
      }

      case property::scale:
        registry.get<transform>(entity).scale = static_cast<float>(luaL_checknumber(state, 3));
        return 0;

      case property::angle:
        registry.get<transform>(entity).angle = static_cast<float>(luaL_checknumber(state, 3));
        return 0;

      case property::alpha:
        registry.get<transform>(entity).alpha =
          std::clamp(static_cast<float>(luaL_checknumber(state, 3)), .0f, 255.0f);
        return 0;

      case property::shown:
        registry.get<transform>(entity).shown = lua_toboolean(state, 3) != 0;
        return 0;

      default:
        assert(proxy->prototype != LUA_NOREF && "object must have an object reference");

        lua_rawgeti(state, LUA_REGISTRYINDEX, proxy->prototype);
        lua_pushvalue(state, 3);
        lua_setfield(state, -2, key);
        lua_pop(state, 1);
        return 0;
    }
  }

  int object_gc(lua_State* state) {
    auto* proxy = static_cast<scriptable*>(lua_touserdata(state, 1));

    luaL_unref(state, LUA_REGISTRYINDEX, proxy->on_animation_begin);
    proxy->on_animation_begin = LUA_NOREF;

    luaL_unref(state, LUA_REGISTRYINDEX, proxy->on_animation_end);
    proxy->on_animation_end = LUA_NOREF;

    luaL_unref(state, LUA_REGISTRYINDEX, proxy->on_loop);
    proxy->on_loop = LUA_NOREF;

    luaL_unref(state, LUA_REGISTRYINDEX, proxy->on_collision_begin);
    proxy->on_collision_begin = LUA_NOREF;

    luaL_unref(state, LUA_REGISTRYINDEX, proxy->on_collision_end);
    proxy->on_collision_end = LUA_NOREF;

    luaL_unref(state, LUA_REGISTRYINDEX, proxy->on_wake);
    proxy->on_wake = LUA_NOREF;

    luaL_unref(state, LUA_REGISTRYINDEX, proxy->on_sleep);
    proxy->on_sleep = LUA_NOREF;

    luaL_unref(state, LUA_REGISTRYINDEX, proxy->on_screen_exit);
    proxy->on_screen_exit = LUA_NOREF;

    luaL_unref(state, LUA_REGISTRYINDEX, proxy->on_screen_enter);
    proxy->on_screen_enter = LUA_NOREF;

    luaL_unref(state, LUA_REGISTRYINDEX, proxy->on_spawn);
    proxy->on_spawn = LUA_NOREF;

    luaL_unref(state, LUA_REGISTRYINDEX, proxy->kind_ref);
    proxy->kind_ref = LUA_NOREF;

    luaL_unref(state, LUA_REGISTRYINDEX, proxy->name_ref);
    proxy->name_ref = LUA_NOREF;

    luaL_unref(state, LUA_REGISTRYINDEX, proxy->prototype);
    proxy->prototype = LUA_NOREF;

    return 0;
  }
}


void object::bind(scriptable& proxy, std::string_view name, std::string_view kind) {
  depot->source.insert(kind);

  lua_pushlstring(L, name.data(), name.size());
  proxy.name_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_pushlstring(L, kind.data(), kind.size());
  proxy.kind_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  pcall(L, 0, 1);

  proxy.prototype = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.prototype);

  lua_getfield(L, -1, "on_loop");
  proxy.on_loop = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_animation_end");
  proxy.on_animation_end = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_animation_begin");
  proxy.on_animation_begin = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_collision_begin");
  proxy.on_collision_begin = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_collision_end");
  proxy.on_collision_end = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_wake");
  proxy.on_wake = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_sleep");
  proxy.on_sleep = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_screen_exit");
  proxy.on_screen_exit = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_screen_enter");
  proxy.on_screen_enter = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_spawn");
  proxy.on_spawn = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_pop(L, 1);

  auto* memory = static_cast<scriptable*>(lua_newuserdata(L, sizeof(scriptable)));
  std::memcpy(memory, &proxy, sizeof(scriptable));
  luaL_getmetatable(L, "Object");
  lua_setmetatable(L, -2);

  proxy.handle = luaL_ref(L, LUA_REGISTRYINDEX);
  memory->handle = proxy.handle;
}

void object::wire() {
  metatable(L, "Object", object_index, guard<object_newindex>, object_gc);

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
