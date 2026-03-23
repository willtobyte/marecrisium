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
    const auto* proxy = argument<objectproxy>(state, 1, "Object");
    const auto key = argument<std::string_view>(state, 2);

    if (key == "alive")
      return push(state, proxy->registry->valid(proxy->entity));

    if (!proxy->registry->valid(proxy->entity)) [[unlikely]]
      return lua_pushnil(state), 1;

    auto& registry = *proxy->registry;
    const auto entity = proxy->entity;

    if (key == "x")
      return push(state, registry.get<transform>(entity).x);

    if (key == "y")
      return push(state, registry.get<transform>(entity).y);

    if (key == "z")
      return push(state, registry.get<renderable>(entity).z);

    if (key == "vx") {
      const auto* bd = registry.try_get<body>(entity);
      if (bd && b2Body_IsValid(bd->id))
        return push(state, b2Body_GetLinearVelocity(bd->id).x);
      return push(state, .0f);
    }

    if (key == "vy") {
      const auto* bd = registry.try_get<body>(entity);
      if (bd && b2Body_IsValid(bd->id))
        return push(state, b2Body_GetLinearVelocity(bd->id).y);
      return push(state, .0f);
    }

    if (key == "flip") {
      switch (registry.get<transform>(entity).flip) {
        case flipmode::horizontal:
          return push(state, "horizontal");
        case flipmode::vertical:
          return push(state, "vertical");
        case flipmode::both:
          return push(state, "both");
        default:
          return push(state, "none");
      }
    }

    if (key == "dormant")
      return push(state, registry.all_of<dormant>(entity));

    if (key == "grounded")
      return push(state, registry.all_of<grounded>(entity));

    if (key == "animation") {
      if (registry.all_of<animation>(entity)) {
        const auto& a = registry.get<animation>(entity);
        if (a.playing && a.clip_count > 0) {
          const auto* strings = registry.ctx().get<stringpool*>();
          return push(state, strings->get(a.clips[a.active].name));
        }
      }

      return lua_pushnil(state), 1;
    }

    if (key == "shown")
      return push(state, registry.get<transform>(entity).shown);

    if (key == "scale")
      return push(state, registry.get<transform>(entity).scale);

    if (key == "angle")
      return push(state, registry.get<transform>(entity).angle);

    if (key == "alpha")
      return push(state, registry.get<transform>(entity).alpha);

    if (key == "riding") {
      const auto* rd = registry.try_get<riding>(entity);
      if (rd && rd->target != entt::null && registry.valid(rd->target) && registry.all_of<objectproxy>(rd->target)) {
        const auto* strings = registry.ctx().get<stringpool*>();
        const auto& target_proxy = registry.get<objectproxy>(rd->target);
        return push(state, strings->get(target_proxy.name));
      }

      return lua_pushnil(state), 1;
    }

    if (key == "name") {
      const auto* strings = registry.ctx().get<stringpool*>();
      return push(state, strings->get(proxy->name));
    }

    if (key == "kind") {
      const auto* strings = registry.ctx().get<stringpool*>();
      return push(state, strings->get(proxy->kind));
    }

    if (key == "position") {
      const auto& tf = registry.get<transform>(entity);
      pushvec2(state, tf.x, tf.y);
      return 1;
    }

    assert(proxy->prototype != LUA_NOREF && "object must have an object reference");

    return dispatch(state, proxy->prototype, key);
  }

  int object_newindex(lua_State* state) {
    auto* proxy = argument<objectproxy>(state, 1, "Object");
    const auto key = argument<std::string_view>(state, 2);

    if (!proxy->registry->valid(proxy->entity))
      return 0;

    auto& registry = *proxy->registry;
    const auto entity = proxy->entity;

    if (key == "x") {
      auto& tf = registry.get<transform>(entity);
      tf.x = argument<float>(state, 3);

      auto* bd = registry.try_get<body>(entity);
      if (bd && bd->type != body_type::kinematic && b2Body_IsValid(bd->id))
        sync_body_position(*bd, tf, registry.try_get<animation>(entity));

      return 0;
    }

    if (key == "y") {
      auto& tf = registry.get<transform>(entity);
      tf.y = argument<float>(state, 3);

      auto* bd = registry.try_get<body>(entity);
      if (bd && bd->type != body_type::kinematic && b2Body_IsValid(bd->id))
        sync_body_position(*bd, tf, registry.try_get<animation>(entity));

      return 0;
    }

    if (key == "z") {
      auto& r = registry.get<renderable>(entity);
      const auto value = argument<int>(state, 3);
      r.z = value;

      return 0;
    }

    if (key == "vx") {
      auto* bd = registry.try_get<body>(entity);
      if (bd && bd->type == body_type::dynamic && b2Body_IsValid(bd->id)) {
        const auto current = b2Body_GetLinearVelocity(bd->id);
        b2Body_SetLinearVelocity(bd->id, {argument<float>(state, 3), current.y});
      }

      return 0;
    }

    if (key == "vy") {
      auto* bd = registry.try_get<body>(entity);
      if (bd && bd->type == body_type::dynamic && b2Body_IsValid(bd->id)) {
        const auto current = b2Body_GetLinearVelocity(bd->id);
        b2Body_SetLinearVelocity(bd->id, {current.x, argument<float>(state, 3)});
      }

      return 0;
    }

    if (key == "flip") {
      const auto value = argument<std::string_view>(state, 3);
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
      if (registry.all_of<animation>(entity)) {
        const auto value = argument<std::string_view>(state, 3);
        const auto hash = entt::hashed_string{value.data()}.value();

        auto& a = registry.get<animation>(entity);

        for (uint8_t i = 0; i < a.clip_count; ++i) {
          if (a.clips[i].name == hash) {
            const auto previous = a.playing ? a.clips[a.active].name : entt::id_type{};
            a.active = i;
            a.current = 0;
            a.elapsed = .0f;
            a.playing = true;

            if (a.clips[i].fx) {
              a.clips[i].fx->play();
            }

            if (proxy->handle != LUA_NOREF) {
              const auto* strings = registry.ctx().get<stringpool*>();

              if (previous != 0 && previous != hash)
                invoke(state, proxy->on_animation_end, proxy->handle, strings->get(previous));

              invoke(state, proxy->on_animation_begin, proxy->handle, strings->get(hash));
            }

            return 0;
          }
        }
      }

      return 0;
    }

    if (key == "scale") {
      registry.get<transform>(entity).scale = argument<float>(state, 3);
      return 0;
    }

    if (key == "angle") {
      registry.get<transform>(entity).angle = argument<float>(state, 3);
      return 0;
    }

    if (key == "alpha") {
      registry.get<transform>(entity).alpha =
        std::clamp(argument<float>(state, 3), .0f, 255.0f);
      return 0;
    }

    if (key == "shown") {
      registry.get<transform>(entity).shown = argument<bool>(state, 3);
      return 0;
    }

    if (key == "position") {
      const auto [px, py] = argument<std::pair<float, float>>(state, 3);

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
    auto* proxy = argument<objectproxy>(state, 1, "Object");
    release(state, proxy->on_animation_begin);
    release(state, proxy->on_animation_end);
    release(state, proxy->on_loop);
    release(state, proxy->prototype);

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

  on_loop = acquire(L, -1, "on_loop");
  on_animation_end = acquire(L, -1, "on_animation_end");
  on_animation_begin = acquire(L, -1, "on_animation_begin");

  lua_pop(L, 1);

  auto* memory = lua_newuserdata(L, sizeof(objectproxy));
  std::memcpy(memory, this, sizeof(objectproxy));
  luaL_getmetatable(L, "Object");
  lua_setmetatable(L, -2);

  handle = luaL_ref(L, LUA_REGISTRYINDEX);
}

void object::wire() {
  metatable(L, "Object", object_index, object_newindex, object_gc);
}
