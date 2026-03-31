#include "stage.hpp"

static void* to_userdata(entt::entity e) noexcept {
  return reinterpret_cast<void*>(static_cast<uintptr_t>(e) + 1);
}

static entt::entity to_entity(const void* p) noexcept {
  return static_cast<entt::entity>(reinterpret_cast<uintptr_t>(p) - 1);
}

static const auto filter = b2DefaultQueryFilter();

static b2Vec2 body_center(const transform& tf, const frame& fr, const body& bd) noexcept {
  return {tf.x + fr.cx + bd.cached_hx,
          tf.y + fr.cy + bd.cached_hy};
}

static constexpr auto mapping(std::string_view s) noexcept -> std::pair<body_type, b2BodyType> {
  if (s == "dynamic") return {body_type::dynamic, b2_dynamicBody};
  if (s == "static") return {body_type::stationary, b2_staticBody};
  return {body_type::kinematic, b2_kinematicBody};
}

static bool ensure_shape(body &bd, const frame &fr, entt::entity en, const transform &tf, float timestep) noexcept {
  const auto hx = fr.cw * .5f;
  const auto hy = fr.ch * .5f;

  if (!b2Shape_IsValid(bd.shape)) {
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

    const auto center = body_center(tf, fr, bd);

    if (bd.type == body_type::kinematic)
      b2Body_SetTargetTransform(bd.id, {center, b2Rot_identity}, timestep);
    else
      b2Body_SetTransform(bd.id, center, b2Rot_identity);

    return true;
  }

  if (hx != bd.cached_hx || hy != bd.cached_hy) [[unlikely]] {
    const auto polygon = b2MakeBox(hx, hy);
    b2Shape_SetPolygon(bd.shape, &polygon);
    bd.cached_hx = hx;
    bd.cached_hy = hy;
  }

  if (bd.type == body_type::kinematic) {
    const auto center = body_center(tf, fr, bd);
    b2Body_SetTargetTransform(bd.id, {center, b2Rot_identity}, timestep);
  }

  return false;
}

static bool resolve(b2ShapeId a, b2ShapeId b, entt::entity &ea, entt::entity &eb) noexcept {
  if (!b2Shape_IsValid(a) || !b2Shape_IsValid(b)) [[unlikely]]
    return false;

  const auto *ua = b2Shape_GetUserData(a);
  const auto *ub = b2Shape_GetUserData(b);
  if (!ua || !ub) [[unlikely]]
    return false;

  ea = to_entity(ua);
  eb = to_entity(ub);
  return true;
}

static void on_objectproxy_destroy(entt::registry& registry, entt::entity entity) {
  auto& proxy = registry.get<objectproxy>(entity);

  if (proxy.handle == LUA_NOREF)
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.handle);
  auto* userdata = static_cast<objectproxy*>(lua_touserdata(L, -1));
  if (userdata) {
    luaL_unref(L, LUA_REGISTRYINDEX, userdata->on_animation_begin);
    userdata->on_animation_begin = LUA_NOREF;
    luaL_unref(L, LUA_REGISTRYINDEX, userdata->on_animation_end);
    userdata->on_animation_end = LUA_NOREF;
    luaL_unref(L, LUA_REGISTRYINDEX, userdata->on_loop);
    userdata->on_loop = LUA_NOREF;
    luaL_unref(L, LUA_REGISTRYINDEX, userdata->prototype);
    userdata->prototype = LUA_NOREF;
  }

  lua_pop(L, 1);
  luaL_unref(L, LUA_REGISTRYINDEX, proxy.handle);
  proxy.handle = LUA_NOREF;
}

static void on_object_destroy(entt::registry& registry, entt::entity entity) {
  auto& bo = registry.get<body>(entity);
  if (b2Body_IsValid(bo.id))
    b2DestroyBody(bo.id);
}

static void overlap_aabb(b2WorldId world, b2AABB aabb, std::vector<entt::entity> &hits) noexcept {
  b2World_OverlapAABB(
    world,
    aabb,
    filter,
    +[](b2ShapeId shape, void *userdata) -> bool {
      auto *hits = static_cast<std::vector<entt::entity> *>(userdata);
      hits->emplace_back(to_entity(b2Shape_GetUserData(shape)));
      return true;
    },
    &hits);
}

static int push(lua_State *state, entt::registry& registry, std::span<const entt::entity> entities, entt::entity caller = entt::null, entt::id_type kind = 0, bool filter_kind = false) {
  lua_newtable(state);
  int index = 1;

  for (const auto entity : entities) {
    if (entity == caller)
      continue;
    if (!registry.valid(entity)
        || !registry.all_of<objectproxy>(entity)) [[unlikely]]
      continue;
    const auto& proxy = registry.get<objectproxy>(entity);
    if (proxy.handle == LUA_NOREF)
      continue;
    if (filter_kind && proxy.kind != kind)
      continue;

    lua_rawgeti(state, LUA_REGISTRYINDEX, proxy.handle);
    lua_rawseti(state, -2, index++);
  }

  return 1;
}

static int world_spawn(lua_State* state) {
  auto* self = static_cast<stage *>(lua_touserdata(state, lua_upvalueindex(1)));
  const auto name = std::string_view{luaL_checkstring(state, 1)};
  const auto kind = std::string_view{luaL_checkstring(state, 2)};
  const auto x = static_cast<float>(luaL_checknumber(state, 3));
  const auto y = static_cast<float>(luaL_checknumber(state, 4));
  return self->spawn(state, name, kind, x, y);
}

static int world_destroy(lua_State* state) {
  auto* self = static_cast<stage *>(lua_touserdata(state, lua_upvalueindex(1)));
  return self->destroy(state);
}

static int world_at(lua_State* state) {
  auto* self = static_cast<stage *>(lua_touserdata(state, lua_upvalueindex(1)));
  const auto x = static_cast<float>(luaL_checknumber(state, 1));
  const auto y = static_cast<float>(luaL_checknumber(state, 2));
  return self->at(state, x, y);
}

static int world_count(lua_State* state) {
  auto* self = static_cast<stage *>(lua_touserdata(state, lua_upvalueindex(1)));
  return self->count(state);
}

static int world_find(lua_State* state) {
  auto* self = static_cast<stage *>(lua_touserdata(state, lua_upvalueindex(1)));
  return self->find(state);
}

static int world_radar(lua_State* state) {
  auto* self = static_cast<stage *>(lua_touserdata(state, lua_upvalueindex(1)));
  const auto* caller = static_cast<objectproxy *>(luaL_checkudata(state, 1, "Object"));
  const auto x = static_cast<float>(luaL_checknumber(state, 2));
  const auto y = static_cast<float>(luaL_checknumber(state, 3));
  const auto radius = static_cast<float>(luaL_checknumber(state, 4));
  return self->radar(state, caller->entity, x, y, radius);
}

static int world_raycast(lua_State* state) {
  auto* self = static_cast<stage *>(lua_touserdata(state, lua_upvalueindex(1)));
  const auto* caller = static_cast<objectproxy *>(luaL_checkudata(state, 1, "Object"));
  const auto x = static_cast<float>(luaL_checknumber(state, 2));
  const auto y = static_cast<float>(luaL_checknumber(state, 3));
  const auto angle = static_cast<float>(luaL_checknumber(state, 4));
  const auto distance = static_cast<float>(luaL_checknumber(state, 5));
  return self->raycast(state, caller->entity, x, y, angle, distance);
}

static int world_pathfind(lua_State* state) {
  auto* self = static_cast<stage *>(lua_touserdata(state, lua_upvalueindex(1)));
  const auto x1 = static_cast<float>(luaL_checknumber(state, 1));
  const auto y1 = static_cast<float>(luaL_checknumber(state, 2));
  const auto x2 = static_cast<float>(luaL_checknumber(state, 3));
  const auto y2 = static_cast<float>(luaL_checknumber(state, 4));
  const auto r  = static_cast<float>(luaL_checknumber(state, 5));
  return self->pathfind(state, x1, y1, x2, y2, r);
}

stage::stage(std::string_view name)
    : _name(name) {
  _registry.on_destroy<objectproxy>().connect<&on_objectproxy_destroy>();
  _registry.on_destroy<body>().connect<&on_object_destroy>();
  _registry.ctx().emplace<stringpool*>(&_stringpool);
  _registry.ctx().emplace<reorder>();

  const auto filename = std::format("stages/{}.lua", name);
  const auto buffer = io::read(filename);
  const auto label = std::format("@{}", filename);
  compile(L, buffer, label);

  lua_newtable(L);
  _pool_reference = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_newtable(L);

  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, world_spawn, 1);
  lua_setfield(L, -2, "spawn");

  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, world_destroy, 1);
  lua_setfield(L, -2, "destroy");

  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, world_at, 1);
  lua_setfield(L, -2, "at");

  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, world_count, 1);
  lua_setfield(L, -2, "count");

  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, world_find, 1);
  lua_setfield(L, -2, "find");

  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, world_radar, 1);
  lua_setfield(L, -2, "radar");

  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, world_raycast, 1);
  lua_setfield(L, -2, "raycast");

  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, world_pathfind, 1);
  lua_setfield(L, -2, "pathfind");

  _world_reference = luaL_ref(L, LUA_REGISTRYINDEX);

  pcall(L, 0, 1);

  b2Vec2 gravity{.0f, .0f};
  lua_getfield(L, -1, "gravity");
  if (lua_istable(L, -1)) {
    lua_rawgeti(L, -1, 1);
    gravity.x = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);
    lua_rawgeti(L, -1, 2);
    gravity.y = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  b2WorldDef def = b2DefaultWorldDef();
  def.gravity = gravity;
  _world = b2CreateWorld(&def);

  const auto largest = std::max(viewport.width, viewport.height);
  _sleep_margin = largest * 1.f;
  _wake_margin  = largest * .5f;

  lua_getfield(L, -1, "objects");
  if (lua_istable(L, -1)) {
    const auto count = static_cast<int>(lua_objlen(L, -1));

    for (int i = 1; i <= count; ++i) {
      lua_rawgeti(L, -1, i);

      lua_getfield(L, -1, "name");
      const auto *name_raw = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
      const auto object_name = std::string(name_raw);
      lua_pop(L, 1);

      lua_getfield(L, -1, "kind");
      const auto *kind_raw = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
      const auto object_kind = std::string(kind_raw);
      lua_pop(L, 1);

      lua_getfield(L, -1, "x");
      const auto ox = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : .0f;
      lua_pop(L, 1);

      lua_getfield(L, -1, "y");
      const auto oy = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : .0f;
      lua_pop(L, 1);

      lua_pop(L, 1);

      std::ignore = spawn(L, object_name, object_kind, ox, oy);
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

      if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        continue;
      }

      lua_getfield(L, -1, "name");
      const auto *snd_raw = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
      const auto sound_name = std::string(snd_raw);
      lua_pop(L, 1);

      lua_getfield(L, -1, "autoplay");
      const auto autoplay = lua_isboolean(L, -1) ? lua_toboolean(L, -1) != 0 : false;
      lua_pop(L, 1);

      lua_getfield(L, -1, "loop");
      const auto loop = lua_isboolean(L, -1) ? lua_toboolean(L, -1) != 0 : false;
      lua_pop(L, 1);

      auto *instance = depot->sound.get(std::format("sounds/{}", sound_name));

      auto **memory = static_cast<class sound **>(lua_newuserdata(L, sizeof(class sound *)));
      *memory = instance;
      luaL_getmetatable(L, "Sound");
      lua_setmetatable(L, -2);

      lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_reference);
      lua_pushvalue(L, -2);
      lua_setfield(L, -2, sound_name.data());
      lua_pop(L, 1);

      _sounds.emplace_back(instance);
      lua_pop(L, 1);

      if (loop)
        instance->set_loop(true);

      if (autoplay)
        instance->play();

      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);

  lua_getfield(L, -1, "tilemap");
  const auto *tilemap_raw = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
  const auto tilemap_name = tilemap_raw ? std::string_view{tilemap_raw} : std::string_view{};
  lua_pop(L, 1);
  if (!tilemap_name.empty())
    _tilemap = tilemap(tilemap_name, _world);

  lua_getfield(L, -1, "overlay");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "widgets");
    const auto *w_raw = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
    const auto widgets = w_raw ? std::string_view{w_raw} : std::string_view{};
    lua_pop(L, 1);
    if (!widgets.empty())
      _overlay = std::string{widgets};

    lua_getfield(L, -1, "foreground");
    const auto *fg_raw = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
    const auto fg = fg_raw ? std::string_view{fg_raw} : std::string_view{};
    lua_pop(L, 1);
    if (!fg.empty())
      _foreground = std::string{fg};
  }
  lua_pop(L, 1);

  lua_getfield(L, -1, "particles");
  if (lua_istable(L, -1)) {
    const auto count = static_cast<int>(lua_objlen(L, -1));

    for (int i = 1; i <= count; ++i) {
      lua_rawgeti(L, -1, i);

      lua_getfield(L, -1, "name");
      const auto *pn_raw = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
      const auto particle_name = std::string(pn_raw);
      lua_pop(L, 1);

      lua_getfield(L, -1, "kind");
      const auto *pk_raw = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
      const auto particle_kind = std::string(pk_raw);
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

      lua_getfield(L, -1, "sound");
      const auto *ps_raw = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
      const auto sound_name = ps_raw ? std::string_view{ps_raw} : std::string_view{};
      lua_pop(L, 1);

      lua_getfield(L, -1, "distance");
      const auto particle_distance = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : 300.f;
      lua_pop(L, 1);

      lua_getfield(L, -1, "volume");
      const auto particle_volume = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : 1.f;
      lua_pop(L, 1);

      lua_pop(L, 1);

      auto *particle = _particlesystem.add(particle_name, particle_kind, px, py, active);

      if (!sound_name.empty()) {
        auto *fx = depot->sound.get(std::format("sounds/{}", sound_name));
        fx->set_loop(true);

        particle->set_sound(fx, particle_distance, particle_volume);
      }

      auto **pmem = static_cast<class particle **>(lua_newuserdata(L, sizeof(class particle *)));
      *pmem = particle;
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

  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);

  lua_getfield(L, -1, "on_loop");
  _on_loop = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_camera");
  _on_camera = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_tick");
  _on_tick = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_pop(L, 1);
}

stage::~stage() {
  luaL_unref(L, LUA_REGISTRYINDEX, _on_tick);
  _on_tick = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, _on_camera);
  _on_camera = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, _on_loop);
  _on_loop = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, _world_reference);
  _world_reference = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, _pool_reference);
  _pool_reference = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, _reference);
  _reference = LUA_NOREF;

  b2DestroyWorld(_world);
}

void stage::update(float delta) {
  for (auto&& [entity, proxy, tf, an] :
       _registry.view<sleepable, objectproxy, transform, animation>().each()) {
    const auto& frame = an.clips[an.active].frames[an.current];
    const auto width  = frame.w * tf.scale;
    const auto height = frame.h * tf.scale;
    const auto screen_x = tf.x - viewport.x;
    const auto screen_y = tf.y - viewport.y;

    const auto offscreen =
      screen_x + width  < -_sleep_margin ||
      screen_x          >  viewport.width  + _sleep_margin ||
      screen_y + height < -_sleep_margin ||
      screen_y          >  viewport.height + _sleep_margin;

    const auto nearscreen =
      screen_x + width  >= -_wake_margin &&
      screen_x          <   viewport.width  + _wake_margin &&
      screen_y + height >= -_wake_margin &&
      screen_y          <   viewport.height + _wake_margin;

    const auto is_dormant = _registry.all_of<dormant>(entity);

    if (nearscreen && is_dormant) {
      if (auto* bd = _registry.try_get<body>(entity);
          bd && b2Body_IsValid(bd->id))
        b2Body_Enable(bd->id);

      _registry.remove<dormant>(entity);

      if (proxy.prototype == LUA_NOREF || proxy.handle == LUA_NOREF) [[unlikely]]
        continue;

      lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.prototype);
      lua_getfield(L, -1, "on_wake");
      if (lua_isfunction(L, -1)) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.handle);
        pcall(L, 1, 0);
      } else {
        lua_pop(L, 1);
      }
      lua_pop(L, 1);
      continue;
    }

    if (offscreen && !is_dormant) {
      _registry.emplace<dormant>(entity);

      if (auto* bd = _registry.try_get<body>(entity);
          bd && b2Body_IsValid(bd->id))
        b2Body_Disable(bd->id);

      if (proxy.prototype == LUA_NOREF || proxy.handle == LUA_NOREF) [[unlikely]]
        continue;

      lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.prototype);
      lua_getfield(L, -1, "on_sleep");
      if (lua_isfunction(L, -1)) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.handle);
        pcall(L, 1, 0);
      } else {
        lua_pop(L, 1);
      }
      lua_pop(L, 1);
    }
  }

  if (_on_loop != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_loop);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
    lua_pushnumber(L, static_cast<lua_Number>(delta));
    pcall(L, 2, 0);
  }

  for (auto&& [entity, proxy] : _registry.view<objectproxy>(entt::exclude<dormant>).each()) {
    if (proxy.prototype == LUA_NOREF || proxy.handle == LUA_NOREF) [[unlikely]]
      continue;

    if (proxy.on_loop != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.on_loop);
      lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.handle);
      lua_pushnumber(L, static_cast<lua_Number>(delta));
      pcall(L, 2, 0);
    }

    auto* a = _registry.try_get<animation>(entity);
    if (!a || !a->playing || a->clip_count == 0) [[unlikely]]
      continue;

    const auto& c = a->clips[a->active];
    const auto& fr = c.frames[a->current];
    if (c.count == 0 || fr.duration < .0f) [[unlikely]]
      continue;

    a->elapsed += delta;

    if (a->elapsed >= fr.duration) {
      a->elapsed -= fr.duration;
      ++a->current;

      if (a->current >= c.count) {
        a->current = 0;

        if (proxy.on_animation_end != LUA_NOREF) {
          lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.on_animation_end);
          lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.handle);
          lua_pushstring(L, _stringpool.get(c.name));
          pcall(L, 2, 0);
        }

        if (proxy.on_animation_begin != LUA_NOREF) {
          lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.on_animation_begin);
          lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.handle);
          lua_pushstring(L, _stringpool.get(c.name));
          pcall(L, 2, 0);
        }
      }
    }
  }

  _accumulator += delta;

  while (_accumulator >= _timestep) {
    for (auto&& [en, bd, an, tf] :
         _registry.view<body, animation, transform>(entt::exclude<dormant>).each()) {
      if (bd.type == body_type::dynamic) {
        tf.previous_x = tf.x;
        tf.previous_y = tf.y;
      }

      const auto& frame = an.clips[an.active].frames[an.current];

      if (ensure_shape(bd, frame, en, tf, _timestep))
        continue;
    }

    b2World_Step(_world, _timestep, _substeps);

    const auto events = b2World_GetBodyEvents(_world);

    for (const auto& event : std::span(events.moveEvents, static_cast<size_t>(events.moveCount))) {
      const auto entity = to_entity(event.userData);

      if (!_registry.valid(entity)) [[unlikely]]
        continue;

      const auto* bd = _registry.try_get<body>(entity);
      if (!bd || bd->type != body_type::dynamic) [[unlikely]]
        continue;

      const auto* an = _registry.try_get<animation>(entity);
      if (!an || !an->playing || an->clip_count == 0) [[unlikely]]
        continue;

      auto& tf = _registry.get<transform>(entity);
      const auto& frame = an->clips[an->active].frames[an->current];
      const auto position = event.transform.p;
      tf.x = position.x - frame.cx * tf.scale - bd->cached_hx;
      tf.y = position.y - frame.cy * tf.scale - bd->cached_hy;

      _registry.get<renderable>(entity).z = static_cast<int>(tf.y + frame.h * tf.scale);
    }

    if (events.moveCount > 0) [[likely]]
      _registry.ctx().get<reorder>().dirty = true;

    _interpolation.previous = _interpolation.current;

    if (_on_camera != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, _on_camera);
      lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);

      pcall(L, 1, 2);

      if (lua_isnumber(L, -2))
        _interpolation.current.x = static_cast<float>(lua_tonumber(L, -2));
      if (lua_isnumber(L, -1))
        _interpolation.current.y = static_cast<float>(lua_tonumber(L, -1));
      lua_pop(L, 2);
    }

    if (!_interpolation.ready) [[unlikely]] {
      _interpolation.previous = _interpolation.current;
      _interpolation.ready = true;
    }

    _accumulator -= _timestep;
  }

  _interpolation.alpha = _timestep > .0f ? _accumulator / _timestep : .0f;

  static constexpr std::string_view directions[] = {"left", "right", "top", "bottom"};

  const auto viewport_right  = viewport.x + viewport.width;
  const auto viewport_bottom = viewport.y + viewport.height;

  for (auto&& [entity, bd, sb, proxy] : _registry.view<const body, boundary, const objectproxy>(entt::exclude<dormant>).each()) {
    if (!b2Body_IsValid(bd.id) || !b2Shape_IsValid(bd.shape)) [[unlikely]]
      continue;

    const auto aabb = b2Shape_GetAABB(bd.shape);

    uint8_t current = 0;
    if (aabb.upperBound.x < viewport.x)
      current |= boundary::left;
    if (aabb.lowerBound.x > viewport_right)
      current |= boundary::right;
    if (aabb.upperBound.y < viewport.y)
      current |= boundary::top;
    if (aabb.lowerBound.y > viewport_bottom)
      current |= boundary::bottom;

    const auto exited  = static_cast<uint8_t>(current & ~sb.previous);
    const auto entered = static_cast<uint8_t>(sb.previous & ~current);

    if (proxy.prototype == LUA_NOREF || proxy.handle == LUA_NOREF) [[unlikely]] {
      sb.previous = current;
      continue;
    }

    for (uint8_t bit = 0; bit < 4; ++bit) {
      const auto mask = static_cast<uint8_t>(1u << bit);
      if (exited & mask) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.prototype);
        lua_getfield(L, -1, "on_screen_exit");
        if (lua_isfunction(L, -1)) {
          lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.handle);
          lua_pushlstring(L, directions[bit].data(), directions[bit].size());
          pcall(L, 2, 0);
        } else {
          lua_pop(L, 1);
        }
        lua_pop(L, 1);
      }
      if (entered & mask) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.prototype);
        lua_getfield(L, -1, "on_screen_enter");
        if (lua_isfunction(L, -1)) {
          lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.handle);
          lua_pushlstring(L, directions[bit].data(), directions[bit].size());
          pcall(L, 2, 0);
        } else {
          lua_pop(L, 1);
        }
        lua_pop(L, 1);
      }
    }

    sb.previous = current;
  }

  {
    const auto events = b2World_GetSensorEvents(_world);
    entt::entity ea, eb;

    for (const auto& event : std::span(events.beginEvents, static_cast<size_t>(events.beginCount)))
      if (resolve(event.sensorShapeId, event.visitorShapeId, ea, eb))
        dispatch_collision(ea, eb, "on_collision_begin");

    for (const auto& event : std::span(events.endEvents, static_cast<size_t>(events.endCount)))
      if (resolve(event.sensorShapeId, event.visitorShapeId, ea, eb))
        dispatch_collision(ea, eb, "on_collision_end");

    const auto contact_events = b2World_GetContactEvents(_world);

    for (const auto& event : std::span(contact_events.beginEvents, static_cast<size_t>(contact_events.beginCount))) {
      if (!resolve(event.shapeIdA, event.shapeIdB, ea, eb))
        continue;

      const auto flipped = b2Vec2{-event.manifold.normal.x, -event.manifold.normal.y};
      dispatch_collision(ea, eb, "on_collision_begin", &event.manifold.normal);
      dispatch_collision(eb, ea, "on_collision_begin", &flipped);
    }

    for (const auto& event : std::span(contact_events.endEvents, static_cast<size_t>(contact_events.endCount))) {
      if (!resolve(event.shapeIdA, event.shapeIdB, ea, eb))
        continue;

      dispatch_collision(ea, eb, "on_collision_end");
      dispatch_collision(eb, ea, "on_collision_end");
    }
  }

  for (auto* sound : _sounds) sound->poll();

  _particlesystem.update(delta);

  auto& rd = _registry.ctx().get<reorder>();
  if (rd.dirty) [[unlikely]] {
    _registry.sort<renderable>([](const renderable& lhs, const renderable& rhs) noexcept {
      return lhs.z < rhs.z;
    }, entt::insertion_sort{});

    rd.dirty = false;
  }
}

void stage::draw() {
  const auto cx = _interpolation.previous.x + _interpolation.alpha * (_interpolation.current.x - _interpolation.previous.x);
  const auto cy = _interpolation.previous.y + _interpolation.alpha * (_interpolation.current.y - _interpolation.previous.y);
  viewport.x = std::floor(cx * viewport.scale) / viewport.scale;
  viewport.y = std::floor(cy * viewport.scale) / viewport.scale;

  _tilemap.draw_background();

  auto view = _registry.view<const renderable, const animation, const transform>(entt::exclude<dormant>);
  view.use<renderable>();

  for (auto&& [entity, r, a, tf] : view.each()) {
    if (!tf.shown || !a.playing || !a.pixmap || a.clip_count == 0) [[unlikely]]
      continue;

    const auto& c = a.clips[a.active];
    if (c.count == 0)
      continue;

    const auto& fr = c.frames[a.current];

    const auto* bd = _registry.try_get<body>(entity);
    const auto dynamic = bd && bd->type == body_type::dynamic;
    const auto rx = dynamic ? tf.previous_x + _interpolation.alpha * (tf.x - tf.previous_x) : tf.x;
    const auto ry = dynamic ? tf.previous_y + _interpolation.alpha * (tf.y - tf.previous_y) : tf.y;

    const auto dw = fr.w * tf.scale;
    const auto dh = fr.h * tf.scale;
    const auto sx = std::floor((rx - viewport.x) * viewport.scale) / viewport.scale;
    const auto sy = std::floor((ry - viewport.y) * viewport.scale) / viewport.scale;

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

  _particlesystem.draw();

  _tilemap.draw_foreground();

#ifdef DEBUG
  SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);

  const auto aabb = b2AABB{{viewport.x, viewport.y}, {viewport.x + viewport.width, viewport.y + viewport.height}};

  b2World_OverlapAABB(_world, aabb, filter, +[](b2ShapeId shape, void*) -> bool {
    static const auto margin = .01f * b2GetLengthUnitsPerMeter();
    const auto polygon = b2Shape_GetPolygon(shape);
    const auto position = b2Body_GetPosition(b2Shape_GetBody(shape));
    const auto hx = polygon.vertices[2].x + margin;
    const auto hy = polygon.vertices[2].y + margin;
    const SDL_FRect bounds = {
      position.x - hx - viewport.x,
      position.y - hy - viewport.y,
      hx + hx,
      hy + hy
    };

    SDL_RenderRect(renderer, &bounds);
    return true;
  }, nullptr);

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
#endif
}

void stage::on_enter() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
  lua_getfield(L, -1, "on_enter");
  auto cb = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);
  lua_pop(L, 1);

  if (cb == LUA_NOREF)
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, cb);
  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
  pcall(L, 1, 0);
  luaL_unref(L, LUA_REGISTRYINDEX, cb);
}

void stage::on_leave() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
  lua_getfield(L, -1, "on_leave");
  auto callback = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);
  lua_pop(L, 1);

  if (callback != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
    pcall(L, 1, 0);
    luaL_unref(L, LUA_REGISTRYINDEX, callback);
  }

  conceal();
}

void stage::expose() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_reference);
  lua_setglobal(L, "pool");
  lua_rawgeti(L, LUA_REGISTRYINDEX, _world_reference);
  lua_setglobal(L, "world");
}

void stage::conceal() {
  lua_pushnil(L);
  lua_setglobal(L, "pool");
  lua_pushnil(L);
  lua_setglobal(L, "world");
}

void stage::on_tick(uint64_t tick) {
  if (_on_tick == LUA_NOREF) [[unlikely]]
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, _on_tick);
  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
  lua_pushinteger(L, static_cast<lua_Integer>(tick));
  pcall(L, 2, 0);
}

auto stage::overlay() const noexcept -> const std::optional<std::string>& {
  return _overlay;
}

auto stage::foreground() const noexcept -> const std::optional<std::string>& {
  return _foreground;
}

int stage::spawn(lua_State* state, std::string_view name, std::string_view kind, float x, float y) {
  const auto entity = _registry.create();
  _registry.emplace<renderable>(entity);
  auto& tf = _registry.emplace<transform>(entity);
  tf.previous_x = tf.x = x;
  tf.previous_y = tf.y = y;
  const auto& proxy = _registry.emplace<objectproxy>(entity, _registry, entity, name, kind);
  const auto prototype = proxy.prototype;
  const auto handle = proxy.handle;

  std::ignore = _stringpool.get(name);
  std::ignore = _stringpool.get(kind);

  lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);
  lua_getfield(L, -1, "animation");

  if (lua_istable(L, -1)) {
    auto& a = _registry.emplace<animation>(entity);
    a.pixmap = depot->pixmap.get(std::format("objects/{}", kind));

    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
      if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        continue;
      }

      if (a.clip_count >= a.clips.size()) {
        lua_pop(L, 2);
        break;
      }

      lua_pushvalue(L, -2);
      const auto clip_name = std::string_view{lua_tostring(L, -1)};
      lua_pop(L, 1);
      const auto cid = _stringpool.get(clip_name);

      auto& c = a.clips[a.clip_count];
      c.name = cid;
      c.count = 0;

      {
        const auto frame_count = static_cast<int>(lua_objlen(L, -1));

        for (int f = 1; f <= frame_count && c.count < c.frames.size(); ++f) {
          lua_rawgeti(L, -1, f);

          if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            continue;
          }

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

          lua_pop(L, 1);
        }
      }

      {
        lua_getfield(L, -1, "sound");
        const auto *snd_raw = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
        const auto sound_name = snd_raw ? std::string_view{snd_raw} : std::string_view{};
        lua_pop(L, 1);
        if (!sound_name.empty())
          c.fx = depot->sound.get(std::format("sounds/{}", sound_name));
      }

      ++a.clip_count;
      lua_pop(L, 1);
    }

    if (a.clip_count > 0) {
      a.playing = true;

      lua_getfield(L, -1, "default");
      const auto *def_raw = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
      const auto default_clip = def_raw ? std::string_view{def_raw} : std::string_view{};
      lua_pop(L, 1);
      if (!default_clip.empty()) {
        const auto hash = entt::hashed_string{default_clip.data()}.value();
        for (uint8_t i = 0; i < a.clip_count; ++i) {
          if (a.clips[i].name == hash) {
            a.active = i;
            break;
          }
        }
      }
    }

    const auto& fr = a.clips[a.active].frames[0];
    _registry.get<renderable>(entity).z = static_cast<int>(tf.y + fr.h * tf.scale);
    _registry.ctx().get<reorder>().dirty = true;

    bool collidable = false;
    for (uint8_t ci = 0; ci < a.clip_count && !collidable; ++ci)
      for (uint8_t fi = 0; fi < a.clips[ci].count && !collidable; ++fi)
        collidable = a.clips[ci].frames[fi].collidable;

    if (collidable) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);
      lua_getfield(L, -1, "body");
      const auto *body_raw = lua_isstring(L, -1) ? lua_tostring(L, -1) : nullptr;
      const auto str = body_raw ? std::string_view{body_raw} : std::string_view{};
      lua_pop(L, 1);
      lua_pop(L, 1);

      const auto [type, b2type] = mapping(str);

      b2BodyDef bdef = b2DefaultBodyDef();
      bdef.userData = to_userdata(entity);
      bdef.type = b2type;

      if (type == body_type::dynamic) {
        bdef.isBullet = true;
        bdef.fixedRotation = true;
      }

      const auto id = b2CreateBody(_world, &bdef);
      _registry.emplace<body>(entity, id, b2_nullShapeId, .0f, .0f, type);
      _registry.emplace<boundary>(entity);
    }
  }

  lua_pop(L, 2);

  {
    lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);
    lua_getfield(L, -1, "sleepable");
    const auto wants_sleepable = lua_isboolean(L, -1) ? lua_toboolean(L, -1) != 0 : false;
    lua_pop(L, 1);
    lua_pop(L, 1);

    if (wants_sleepable)
      _registry.emplace<sleepable>(entity);
  }

  if (prototype != LUA_NOREF && handle != LUA_NOREF) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);
    lua_getfield(L, -1, "on_spawn");
    if (lua_isfunction(L, -1)) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, handle);
      pcall(L, 1, 0);
    } else {
      lua_pop(L, 1);
    }
    lua_pop(L, 1);
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_reference);
  lua_rawgeti(L, LUA_REGISTRYINDEX, handle);
  lua_setfield(L, -2, name.data());
  lua_pop(L, 1);

  if (_registry.all_of<sleepable, animation>(entity)) {
    const auto& an = _registry.get<animation>(entity);
    const auto& fr = an.clips[an.active].frames[an.current];
    const auto  w  = fr.w * tf.scale;
    const auto  h  = fr.h * tf.scale;
    const auto  sx = tf.x - viewport.x;
    const auto  sy = tf.y - viewport.y;

    const auto offscreen =
      sx + w < -_sleep_margin ||
      sx     >  viewport.width  + _sleep_margin ||
      sy + h < -_sleep_margin ||
      sy     >  viewport.height + _sleep_margin;

    if (offscreen) {
      _registry.emplace<dormant>(entity);

      if (auto* bd = _registry.try_get<body>(entity);
          bd && b2Body_IsValid(bd->id))
        b2Body_Disable(bd->id);
    }
  }

  lua_rawgeti(state, LUA_REGISTRYINDEX, handle);
  return 1;
}

int stage::destroy(lua_State* state) {
  auto* proxy = static_cast<objectproxy *>(luaL_checkudata(state, 1, "Object"));
  if (!_registry.valid(proxy->entity))
    return 0;

  const auto* object_name = _stringpool.get(proxy->name);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_reference);
  lua_pushnil(L);
  lua_setfield(L, -2, object_name);
  lua_pop(L, 1);

  _registry.destroy(proxy->entity);
  return 0;
}

int stage::at(lua_State *state, float x, float y) {
  constexpr auto HALF = .5f;
  const b2AABB aabb = {{x - HALF, y - HALF}, {x + HALF, y + HALF}};
  std::vector<entt::entity> hits;
  overlap_aabb(_world, aabb, hits);
  return push(state, _registry, hits);
}

int stage::count(lua_State *state) {
  const auto x = static_cast<float>(luaL_checknumber(state, 1));
  const auto y = static_cast<float>(luaL_checknumber(state, 2));
  const auto w = static_cast<float>(luaL_checknumber(state, 3));
  const auto h = static_cast<float>(luaL_checknumber(state, 4));

  const auto filter_kind = lua_isstring(state, 5);
  const auto kind_hash = filter_kind
    ? entt::hashed_string{lua_tostring(state, 5)}.value()
    : entt::id_type{};

  const b2AABB aabb = {{x, y}, {x + w, y + h}};

  std::vector<entt::entity> hits;
  overlap_aabb(_world, aabb, hits);

  int total = 0;

  for (const auto entity : hits) {
    if (!_registry.valid(entity)
        || !_registry.all_of<objectproxy>(entity)) [[unlikely]]
      continue;
    const auto& proxy = _registry.get<objectproxy>(entity);
    if (proxy.handle == LUA_NOREF)
      continue;
    if (filter_kind && proxy.kind != kind_hash)
      continue;

    ++total;
  }

  lua_pushinteger(state, static_cast<lua_Integer>(total));
  return 1;
}

int stage::find(lua_State *state) {
  const auto x = static_cast<float>(luaL_checknumber(state, 1));
  const auto y = static_cast<float>(luaL_checknumber(state, 2));
  const auto w = static_cast<float>(luaL_checknumber(state, 3));
  const auto h = static_cast<float>(luaL_checknumber(state, 4));

  const auto filter_kind = lua_isstring(state, 5);
  const auto kind_hash = filter_kind
    ? entt::hashed_string{lua_tostring(state, 5)}.value()
    : entt::id_type{};

  const b2AABB aabb = {{x, y}, {x + w, y + h}};

  std::vector<entt::entity> hits;
  overlap_aabb(_world, aabb, hits);

  return push(state, _registry, hits, entt::null, kind_hash, filter_kind);
}

int stage::radar(lua_State *state, entt::entity caller, float x, float y, float radius) {
  std::vector<entt::entity> hits;

  const b2Vec2 center{x, y};
  const auto proxy = b2MakeProxy(&center, 1, radius);

  b2World_OverlapShape(
    _world,
    &proxy,
    filter,
    +[](b2ShapeId shape, void *userdata) -> bool {
      auto *hits = static_cast<std::vector<entt::entity> *>(userdata);
      hits->emplace_back(to_entity(b2Shape_GetUserData(shape)));
      return true;
    },
    &hits
  );

  return push(state, _registry, hits, caller);
}

int stage::raycast(lua_State* state, entt::entity caller, float x, float y, float angle, float distance) {
  struct raycast_hit {
    entt::entity entity;
    float fraction;
  };

  const auto radians = to_radians(angle);
  auto sine = .0f, cosine = .0f;
  sincos(radians, sine, cosine);
  const b2Vec2 origin{x, y};
  const b2Vec2 translation{cosine * distance, sine * distance};
  std::vector<raycast_hit> hits;

  b2World_CastRay(
    _world,
    origin,
    translation,
    filter,
    +[](b2ShapeId shape, b2Vec2, b2Vec2, float fraction, void *userdata) -> float {
      auto *hits = static_cast<std::vector<raycast_hit> *>(userdata);
      hits->emplace_back(to_entity(b2Shape_GetUserData(shape)), fraction);
      return 1.f;
    },
    &hits
  );

  std::ranges::sort(hits, {}, &raycast_hit::fraction);

  lua_newtable(state);
  int index = 1;

  for (const auto& [entity, fraction] : hits) {
    if (entity == entt::null)
      continue;

    if (entity == caller)
      continue;

    if (!_registry.valid(entity)
        || !_registry.all_of<objectproxy>(entity)) [[unlikely]]
      continue;

    const auto& proxy = _registry.get<objectproxy>(entity);
    if (proxy.handle == LUA_NOREF)
      continue;

    lua_rawgeti(state, LUA_REGISTRYINDEX, proxy.handle);
    lua_rawseti(state, -2, index++);
  }

  return 1;
}

int stage::pathfind(lua_State* state, float x1, float y1, float x2, float y2, float radius) noexcept {
  return _tilemap.pathfind(state, x1, y1, x2, y2, radius);
}

void stage::dispatch_collision(entt::entity entity, entt::entity other, std::string_view callback, const b2Vec2* normal) {
  if (!_registry.valid(entity)
      || !_registry.valid(other)
      || !_registry.all_of<objectproxy>(entity)
      || !_registry.all_of<objectproxy>(other)) [[unlikely]]
    return;

  const auto& self = _registry.get<objectproxy>(entity);
  const auto& target = _registry.get<objectproxy>(other);
  const auto* name = _stringpool.get(target.name);
  const auto* kind = _stringpool.get(target.kind);

  if (self.prototype == LUA_NOREF || self.handle == LUA_NOREF) [[unlikely]]
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, self.prototype);
  lua_getfield(L, -1, callback.data());
  if (lua_isfunction(L, -1)) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, self.handle);
    lua_pushstring(L, name);
    lua_pushstring(L, kind);
    if (normal) {
      lua_pushnumber(L, static_cast<lua_Number>(normal->x));
      lua_pushnumber(L, static_cast<lua_Number>(normal->y));
      pcall(L, 5, 0);
    } else {
      pcall(L, 3, 0);
    }
  } else {
    lua_pop(L, 1);
  }
  lua_pop(L, 1);
}
