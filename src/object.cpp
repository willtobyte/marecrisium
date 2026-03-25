#include "object.hpp"
#include "sound.hpp"

namespace {
  static void sync_body_position(body& bd, const transform& tf, const animation* an) noexcept {
    auto ox = .0f;
    auto oy = .0f;
    if (an && an->playing && an->clip_count > 0) {
      const auto& fr = an->clips[an->active].frames[an->current];
      ox = fr.cx;
      oy = fr.cy;
    }

    b2Body_SetTransform(bd.id, {tf.x + ox + bd.cached_hx, tf.y + oy + bd.cached_hy}, b2Rot_identity);
  }

  int object_index(lua_State* state) {
    const auto* proxy = static_cast<objectproxy*>(luaL_checkudata(state, 1, "Object"));
    const auto key = std::string_view{luaL_checkstring(state, 2)};

    if (key == "alive") {
      lua_pushboolean(state, proxy->registry->valid(proxy->entity) ? 1 : 0);
      return 1;
    }

    if (!proxy->registry->valid(proxy->entity)) [[unlikely]]
      return lua_pushnil(state), 1;

    auto& registry = *proxy->registry;
    const auto entity = proxy->entity;

    if (key == "x") {
      lua_pushnumber(state, static_cast<lua_Number>(registry.get<transform>(entity).x));
      return 1;
    }

    if (key == "y") {
      lua_pushnumber(state, static_cast<lua_Number>(registry.get<transform>(entity).y));
      return 1;
    }

    if (key == "z") {
      lua_pushinteger(state, static_cast<lua_Integer>(registry.get<renderable>(entity).z));
      return 1;
    }

    if (key == "vx") {
      const auto* bd = registry.try_get<body>(entity);
      if (bd && b2Body_IsValid(bd->id)) {
        lua_pushnumber(state, static_cast<lua_Number>(b2Body_GetLinearVelocity(bd->id).x));
        return 1;
      }
      lua_pushnumber(state, static_cast<lua_Number>(.0f));
      return 1;
    }

    if (key == "vy") {
      const auto* bd = registry.try_get<body>(entity);
      if (bd && b2Body_IsValid(bd->id)) {
        lua_pushnumber(state, static_cast<lua_Number>(b2Body_GetLinearVelocity(bd->id).y));
        return 1;
      }
      lua_pushnumber(state, static_cast<lua_Number>(.0f));
      return 1;
    }

    if (key == "flip") {
      switch (registry.get<transform>(entity).flip) {
        case flipmode::horizontal:
          lua_pushstring(state, "horizontal");
          return 1;
        case flipmode::vertical:
          lua_pushstring(state, "vertical");
          return 1;
        case flipmode::both:
          lua_pushstring(state, "both");
          return 1;
        default:
          lua_pushstring(state, "none");
          return 1;
      }
    }

    if (key == "dormant") {
      lua_pushboolean(state, registry.all_of<dormant>(entity) ? 1 : 0);
      return 1;
    }

    if (key == "grounded") {
      lua_pushboolean(state, registry.all_of<grounded>(entity) ? 1 : 0);
      return 1;
    }

    if (key == "animation") {
      if (!registry.all_of<animation>(entity))
        return lua_pushnil(state), 1;

      const auto& a = registry.get<animation>(entity);
      if (!a.playing || a.clip_count == 0)
        return lua_pushnil(state), 1;

      const auto* strings = registry.ctx().get<stringpool*>();
      lua_pushstring(state, strings->get(a.clips[a.active].name));
      return 1;
    }

    if (key == "shown") {
      lua_pushboolean(state, registry.get<transform>(entity).shown ? 1 : 0);
      return 1;
    }

    if (key == "scale") {
      lua_pushnumber(state, static_cast<lua_Number>(registry.get<transform>(entity).scale));
      return 1;
    }

    if (key == "angle") {
      lua_pushnumber(state, static_cast<lua_Number>(registry.get<transform>(entity).angle));
      return 1;
    }

    if (key == "alpha") {
      lua_pushnumber(state, static_cast<lua_Number>(registry.get<transform>(entity).alpha));
      return 1;
    }

    if (key == "riding") {
      const auto* rd = registry.try_get<riding>(entity);
      if (rd && rd->target != entt::null && registry.valid(rd->target) && registry.all_of<objectproxy>(rd->target)) {
        const auto* strings = registry.ctx().get<stringpool*>();
        const auto& target_proxy = registry.get<objectproxy>(rd->target);
        lua_pushstring(state, strings->get(target_proxy.name));
        return 1;
      }

      return lua_pushnil(state), 1;
    }

    if (key == "name") {
      const auto* strings = registry.ctx().get<stringpool*>();
      lua_pushstring(state, strings->get(proxy->name));
      return 1;
    }

    if (key == "kind") {
      const auto* strings = registry.ctx().get<stringpool*>();
      lua_pushstring(state, strings->get(proxy->kind));
      return 1;
    }

    if (key == "position") {
      const auto& tf = registry.get<transform>(entity);
      lua_createtable(state, 2, 0);
      lua_pushnumber(state, static_cast<lua_Number>(tf.x));
      lua_rawseti(state, -2, 1);
      lua_pushnumber(state, static_cast<lua_Number>(tf.y));
      lua_rawseti(state, -2, 2);
      return 1;
    }

    assert(proxy->prototype != LUA_NOREF && "object must have an object reference");

    return dispatch(state, proxy->prototype, key);
  }

  int object_newindex(lua_State* state) {
    auto* proxy = static_cast<objectproxy*>(luaL_checkudata(state, 1, "Object"));
    const auto key = std::string_view{luaL_checkstring(state, 2)};

    if (!proxy->registry->valid(proxy->entity))
      return 0;

    auto& registry = *proxy->registry;
    const auto entity = proxy->entity;

    if (key == "x") {
      auto& tf = registry.get<transform>(entity);
      tf.x = static_cast<float>(luaL_checknumber(state, 3));

      auto* bd = registry.try_get<body>(entity);
      if (bd && bd->type != body_type::kinematic && b2Body_IsValid(bd->id))
        sync_body_position(*bd, tf, registry.try_get<animation>(entity));

      return 0;
    }

    if (key == "y") {
      auto& tf = registry.get<transform>(entity);
      tf.y = static_cast<float>(luaL_checknumber(state, 3));

      auto* bd = registry.try_get<body>(entity);
      if (bd && bd->type != body_type::kinematic && b2Body_IsValid(bd->id))
        sync_body_position(*bd, tf, registry.try_get<animation>(entity));

      return 0;
    }

    if (key == "z") {
      auto& r = registry.get<renderable>(entity);
      const auto value = static_cast<int>(luaL_checkinteger(state, 3));
      r.z = value;

      return 0;
    }

    if (key == "vx") {
      auto* bd = registry.try_get<body>(entity);
      if (bd && bd->type == body_type::dynamic && b2Body_IsValid(bd->id)) {
        const auto current = b2Body_GetLinearVelocity(bd->id);
        b2Body_SetLinearVelocity(bd->id, {static_cast<float>(luaL_checknumber(state, 3)), current.y});
      }

      return 0;
    }

    if (key == "vy") {
      auto* bd = registry.try_get<body>(entity);
      if (bd && bd->type == body_type::dynamic && b2Body_IsValid(bd->id)) {
        const auto current = b2Body_GetLinearVelocity(bd->id);
        b2Body_SetLinearVelocity(bd->id, {current.x, static_cast<float>(luaL_checknumber(state, 3))});
      }

      return 0;
    }

    if (key == "flip") {
      const auto value = std::string_view{luaL_checkstring(state, 3)};
      auto& tf = registry.get<transform>(entity);
      if (value == "horizontal") {
        tf.flip = flipmode::horizontal;
      } else if (value == "vertical") {
        tf.flip = flipmode::vertical;
      } else if (value == "both") {
        tf.flip = flipmode::both;
      } else if (value == "none") {
        tf.flip = flipmode::none;
      } else {
        return luaL_error(state, "invalid flip value: %s", value.data());
      }

      return 0;
    }

    if (key == "animation") {
      if (!registry.all_of<animation>(entity))
        return 0;

      const auto value = std::string_view{luaL_checkstring(state, 3)};
      const auto hash = entt::hashed_string{value.data()}.value();

      auto& a = registry.get<animation>(entity);

      for (uint8_t i = 0; i < a.clip_count; ++i) {
        if (a.clips[i].name != hash)
          continue;

        const auto previous = a.playing ? a.clips[a.active].name : entt::id_type{};
        a.active = i;
        a.current = 0;
        a.elapsed = .0f;
        a.playing = true;

        if (a.clips[i].fx)
          a.clips[i].fx->play();

        if (proxy->handle == LUA_NOREF)
          return 0;

        const auto* strings = registry.ctx().get<stringpool*>();

        if (previous != 0 && previous != hash && proxy->on_animation_end != LUA_NOREF) {
          lua_rawgeti(state, LUA_REGISTRYINDEX, proxy->on_animation_end);
          lua_rawgeti(state, LUA_REGISTRYINDEX, proxy->handle);
          lua_pushstring(state, strings->get(previous));
          pcall(state, 2, 0);
        }

        if (proxy->on_animation_begin != LUA_NOREF) {
          lua_rawgeti(state, LUA_REGISTRYINDEX, proxy->on_animation_begin);
          lua_rawgeti(state, LUA_REGISTRYINDEX, proxy->handle);
          lua_pushstring(state, strings->get(hash));
          pcall(state, 2, 0);
        }

        return 0;
      }

      return 0;
    }

    if (key == "scale") {
      registry.get<transform>(entity).scale = static_cast<float>(luaL_checknumber(state, 3));
      return 0;
    }

    if (key == "angle") {
      registry.get<transform>(entity).angle = static_cast<float>(luaL_checknumber(state, 3));
      return 0;
    }

    if (key == "alpha") {
      registry.get<transform>(entity).alpha =
        std::clamp(static_cast<float>(luaL_checknumber(state, 3)), .0f, 255.0f);
      return 0;
    }

    if (key == "shown") {
      registry.get<transform>(entity).shown = lua_toboolean(state, 3) != 0;
      return 0;
    }

    if (key == "position") {
      luaL_checktype(state, 3, LUA_TTABLE);
      lua_rawgeti(state, 3, 1);
      lua_rawgeti(state, 3, 2);
      const auto px = static_cast<float>(lua_tonumber(state, -2));
      const auto py = static_cast<float>(lua_tonumber(state, -1));
      lua_pop(state, 2);

      auto& tf = registry.get<transform>(entity);
      tf.x = px;
      tf.y = py;

      auto* bd = registry.try_get<body>(entity);
      if (bd && bd->type != body_type::kinematic && b2Body_IsValid(bd->id))
        sync_body_position(*bd, tf, registry.try_get<animation>(entity));

      return 0;
    }

    assert(proxy->prototype != LUA_NOREF && "object must have an object reference");

    lua_rawgeti(state, LUA_REGISTRYINDEX, proxy->prototype);
    lua_pushvalue(state, 3);
    lua_setfield(state, -2, key.data());
    lua_pop(state, 1);
    return 0;
  }

  int object_gc(lua_State* state) {
    auto* proxy = static_cast<objectproxy*>(luaL_checkudata(state, 1, "Object"));
    luaL_unref(state, LUA_REGISTRYINDEX, proxy->on_animation_begin);
    proxy->on_animation_begin = LUA_NOREF;
    luaL_unref(state, LUA_REGISTRYINDEX, proxy->on_animation_end);
    proxy->on_animation_end = LUA_NOREF;
    luaL_unref(state, LUA_REGISTRYINDEX, proxy->on_loop);
    proxy->on_loop = LUA_NOREF;
    luaL_unref(state, LUA_REGISTRYINDEX, proxy->prototype);
    proxy->prototype = LUA_NOREF;

    return 0;
  }
}


objectproxy::objectproxy(entt::registry& registry, entt::entity entity, std::string_view name, std::string_view kind, int environment)
    : registry(&registry), entity(entity), name(entt::hashed_string{name.data()}.value()), kind(entt::hashed_string{kind.data()}.value()) {
  depot->source.insert(kind);

  lua_rawgeti(L, LUA_REGISTRYINDEX, environment);
  lua_setfenv(L, -2);

  pcall(L, 0, 1);

  prototype = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);

  lua_getfield(L, -1, "on_loop");
  on_loop = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_animation_end");
  on_animation_end = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_animation_begin");
  on_animation_begin = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_pop(L, 1);

  auto* memory = static_cast<objectproxy*>(lua_newuserdata(L, sizeof(objectproxy)));
  std::memcpy(memory, this, sizeof(objectproxy));
  luaL_getmetatable(L, "Object");
  lua_setmetatable(L, -2);

  handle = luaL_ref(L, LUA_REGISTRYINDEX);
  memory->handle = handle;
}

void object::wire() {
  metatable(L, "Object", object_index, object_newindex, object_gc);
}
