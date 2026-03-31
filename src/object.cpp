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
    constexpr auto position = "position"_hs;
  }

  static void sync_body_position(body& bd, const transform& tf, const animation* an) noexcept {
    auto ox = .0f;
    auto oy = .0f;
    if (an && an->playing && an->clip_count > 0) [[likely]] {
      const auto& fr = an->clips[an->active].frames[an->current];
      ox = fr.cx;
      oy = fr.cy;
    }

    b2Body_SetTransform(bd.id, {tf.x + ox + bd.cached_hx, tf.y + oy + bd.cached_hy}, b2Rot_identity);
  }

  int object_index(lua_State* state) {
    const auto* proxy = static_cast<objectproxy*>(luaL_checkudata(state, 1, "Object"));
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
        const auto* bd = registry.try_get<body>(entity);
        if (bd && b2Body_IsValid(bd->id)) [[likely]] {
          lua_pushnumber(state, static_cast<lua_Number>(b2Body_GetLinearVelocity(bd->id).x));
          return 1;
        }
        lua_pushnumber(state, static_cast<lua_Number>(.0f));
        return 1;
      }

      case property::velocity_y: {
        const auto* bd = registry.try_get<body>(entity);
        if (bd && b2Body_IsValid(bd->id)) [[likely]] {
          lua_pushnumber(state, static_cast<lua_Number>(b2Body_GetLinearVelocity(bd->id).y));
          return 1;
        }
        lua_pushnumber(state, static_cast<lua_Number>(.0f));
        return 1;
      }

      case property::flip:
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

      case property::dormant:
        lua_pushboolean(state, registry.all_of<dormant>(entity) ? 1 : 0);
        return 1;

      case property::animation: {
        if (!registry.all_of<animation>(entity))
          return lua_pushnil(state), 1;

        const auto& a = registry.get<animation>(entity);
        if (!a.playing || a.clip_count == 0) [[unlikely]]
          return lua_pushnil(state), 1;

        const auto* strings = registry.ctx().get<stringpool*>();
        lua_pushstring(state, strings->get(a.clips[a.active].name));
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

      case property::name: {
        const auto* strings = registry.ctx().get<stringpool*>();
        lua_pushstring(state, strings->get(proxy->name));
        return 1;
      }

      case property::kind: {
        const auto* strings = registry.ctx().get<stringpool*>();
        lua_pushstring(state, strings->get(proxy->kind));
        return 1;
      }

      case property::position: {
        const auto& tf = registry.get<transform>(entity);
        lua_createtable(state, 2, 0);
        lua_pushnumber(state, static_cast<lua_Number>(tf.x));
        lua_rawseti(state, -2, 1);
        lua_pushnumber(state, static_cast<lua_Number>(tf.y));
        lua_rawseti(state, -2, 2);
        return 1;
      }

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
    auto* proxy = static_cast<objectproxy*>(luaL_checkudata(state, 1, "Object"));
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

        auto* bd = registry.try_get<body>(entity);
        if (bd
            && bd->type != body_type::kinematic
            && b2Body_IsValid(bd->id)) [[likely]]
          sync_body_position(*bd, tf, registry.try_get<animation>(entity));

        return 0;
      }

      case property::y: {
        auto& tf = registry.get<transform>(entity);
        tf.previous_y = tf.y = static_cast<float>(luaL_checknumber(state, 3));

        auto* bd = registry.try_get<body>(entity);
        if (bd
            && bd->type != body_type::kinematic
            && b2Body_IsValid(bd->id)) [[likely]]
          sync_body_position(*bd, tf, registry.try_get<animation>(entity));

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
        auto* bd = registry.try_get<body>(entity);
        if (bd
            && bd->type == body_type::dynamic
            && b2Body_IsValid(bd->id)) [[likely]] {
          const auto current = b2Body_GetLinearVelocity(bd->id);
          b2Body_SetLinearVelocity(bd->id, {static_cast<float>(luaL_checknumber(state, 3)), current.y});
        }

        return 0;
      }

      case property::velocity_y: {
        auto* bd = registry.try_get<body>(entity);
        if (bd
            && bd->type == body_type::dynamic
            && b2Body_IsValid(bd->id)) [[likely]] {
          const auto current = b2Body_GetLinearVelocity(bd->id);
          b2Body_SetLinearVelocity(bd->id, {current.x, static_cast<float>(luaL_checknumber(state, 3))});
        }

        return 0;
      }

      case property::flip: {
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

      case property::animation: {
        if (!registry.all_of<animation>(entity))
          return 0;

        const auto value = std::string_view{luaL_checkstring(state, 3)};
        const auto hash = entt::hashed_string{value.data()};

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

          if (previous != 0
              && previous != hash
              && proxy->on_animation_end != LUA_NOREF) [[unlikely]] {
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

      case property::position: {
        luaL_checktype(state, 3, LUA_TTABLE);
        lua_rawgeti(state, 3, 1);
        lua_rawgeti(state, 3, 2);
        const auto px = static_cast<float>(lua_tonumber(state, -2));
        const auto py = static_cast<float>(lua_tonumber(state, -1));
        lua_pop(state, 2);

        auto& tf = registry.get<transform>(entity);
        tf.previous_x = tf.x = px;
        tf.previous_y = tf.y = py;

        auto* bd = registry.try_get<body>(entity);
        if (bd
            && bd->type != body_type::kinematic
            && b2Body_IsValid(bd->id)) [[likely]]
          sync_body_position(*bd, tf, registry.try_get<animation>(entity));

        return 0;
      }

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


objectproxy::objectproxy(entt::registry& registry, entt::entity entity, std::string_view name, std::string_view kind)
    : registry(&registry), entity(entity), name(entt::hashed_string{name.data()}), kind(entt::hashed_string{kind.data()}) {
  depot->source.insert(kind);

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
