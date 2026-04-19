#include "stage.hpp"

namespace {
  namespace property {
    constexpr auto dynamic_bodytype = "dynamic"_hs;
    constexpr auto static_bodytype  = "static"_hs;
  }
}

static void* to_userdata(entt::entity e) {
  return reinterpret_cast<void*>(static_cast<uintptr_t>(e) + 1);
}

static entt::entity to_entity(const void* p) {
  return static_cast<entt::entity>(reinterpret_cast<uintptr_t>(p) - 1);
}

static void submit(SDL_Texture *texture, std::vector<SDL_Vertex> &vertices, std::vector<int> &indices) {
  SDL_RenderGeometry(
    renderer,
    texture,
    vertices.data(),
    static_cast<int>(vertices.size()),
    indices.data(),
    static_cast<int>(indices.size()));

  vertices.clear();
  indices.clear();
}

static const auto filter = b2DefaultQueryFilter();

static color unpack(lua_State *state, int index) {
  lua_rawgeti(state, index, 1);
  const auto r = static_cast<uint8_t>(lua_tonumber(state, -1));
  lua_pop(state, 1);
  lua_rawgeti(state, index, 2);
  const auto g = static_cast<uint8_t>(lua_tonumber(state, -1));
  lua_pop(state, 1);
  lua_rawgeti(state, index, 3);
  const auto b = static_cast<uint8_t>(lua_tonumber(state, -1));
  lua_pop(state, 1);
  return {r, g, b};
}



static bool culled(const transform &tf, const animation &an, float margin) {
  const auto &fr = an.sheet->frames[an.sheet->clips[an.active].offset + an.current];
  const auto width  = fr.width * tf.scale;
  const auto height = fr.height * tf.scale;
  const auto screen_x = tf.x - viewport.x;
  const auto screen_y = tf.y - viewport.y;
  return screen_x + width  < -margin ||
    screen_x >  viewport.width  + margin ||
    screen_y + height < -margin ||
    screen_y >  viewport.height + margin;
}

static constexpr auto mapping(const char *s) -> std::pair<body_type, b2BodyType> {
  const auto id = entt::hashed_string{s};
  switch (id) {
    case property::dynamic_bodytype: return {body_type::dynamic, b2_dynamicBody};
    case property::static_bodytype:  return {body_type::stationary, b2_staticBody};
    default:                     return {body_type::kinematic, b2_kinematicBody};
  }
}

static bool ensure_shape(body &b, const frame &fr, entt::entity entity, const transform &tf, float timestep) {
  const auto hx = fr.bound_width * .5f;
  const auto hy = fr.bound_height * .5f;

  if (!b2Shape_IsValid(b.shape)) {
    const auto polygon = b2MakeBox(hx, hy);

    auto sdef = b2DefaultShapeDef();
    sdef.userData = to_userdata(entity);
    sdef.enableContactEvents = true;
    sdef.enableSensorEvents = true;

    if (b.type == body_type::dynamic) {
      sdef.density = 1.f;
      sdef.material.friction = .0f;
    }

    b.shape = b2CreatePolygonShape(b.id, &sdef, &polygon);
    b.extent_x = hx;
    b.extent_y = hy;

    const auto center = center_of(b, tf, &fr);

    if (b.type == body_type::kinematic)
      b2Body_SetTargetTransform(b.id, {center, b2Rot_identity}, timestep);
    else
      b2Body_SetTransform(b.id, center, b2Rot_identity);

    return true;
  }

  if (hx != b.extent_x || hy != b.extent_y) [[unlikely]] {
    const auto polygon = b2MakeBox(hx, hy);
    b2Shape_SetPolygon(b.shape, &polygon);
    b.extent_x = hx;
    b.extent_y = hy;
  }

  if (b.type == body_type::kinematic) {
    const auto center = center_of(b, tf, &fr);
    b2Body_SetTargetTransform(b.id, {center, b2Rot_identity}, timestep);
  }

  return false;
}

static bool resolve(b2ShapeId a, b2ShapeId b, entt::entity &ea, entt::entity &eb) {
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

static void on_scriptable_destroy(entt::registry& registry, entt::entity entity) {
  auto& op = registry.get<scriptable>(entity);

  if (op.handle == LUA_NOREF)
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle);
  auto* userdata = static_cast<scriptable*>(lua_touserdata(L, -1));
  if (userdata) {
    luaL_unref(L, LUA_REGISTRYINDEX, userdata->on_spawn);
    userdata->on_spawn = LUA_NOREF;

    luaL_unref(L, LUA_REGISTRYINDEX, userdata->on_screen_enter);
    userdata->on_screen_enter = LUA_NOREF;

    luaL_unref(L, LUA_REGISTRYINDEX, userdata->on_screen_exit);
    userdata->on_screen_exit = LUA_NOREF;

    luaL_unref(L, LUA_REGISTRYINDEX, userdata->on_sleep);
    userdata->on_sleep = LUA_NOREF;

    luaL_unref(L, LUA_REGISTRYINDEX, userdata->on_wake);
    userdata->on_wake = LUA_NOREF;

    luaL_unref(L, LUA_REGISTRYINDEX, userdata->on_collision_end);
    userdata->on_collision_end = LUA_NOREF;

    luaL_unref(L, LUA_REGISTRYINDEX, userdata->on_collision_begin);
    userdata->on_collision_begin = LUA_NOREF;

    luaL_unref(L, LUA_REGISTRYINDEX, userdata->on_animation_begin);
    userdata->on_animation_begin = LUA_NOREF;

    luaL_unref(L, LUA_REGISTRYINDEX, userdata->on_animation_end);
    userdata->on_animation_end = LUA_NOREF;

    luaL_unref(L, LUA_REGISTRYINDEX, userdata->on_loop);
    userdata->on_loop = LUA_NOREF;

    luaL_unref(L, LUA_REGISTRYINDEX, userdata->kind_ref);
    userdata->kind_ref = LUA_NOREF;

    luaL_unref(L, LUA_REGISTRYINDEX, userdata->name_ref);
    userdata->name_ref = LUA_NOREF;

    luaL_unref(L, LUA_REGISTRYINDEX, userdata->prototype);
    userdata->prototype = LUA_NOREF;
  }

  lua_pop(L, 1);
  luaL_unref(L, LUA_REGISTRYINDEX, op.handle);
  op.handle = LUA_NOREF;
}

static void on_object_destroy(entt::registry& registry, entt::entity entity) {
  auto& b = registry.get<body>(entity);
  if (alive(b))
    b2DestroyBody(b.id);
}

static bool gather(b2ShapeId shape, void *userdata) {
  const auto *ud = b2Shape_GetUserData(shape);
  if (!ud) [[unlikely]]
    return true;

  auto *hits = static_cast<std::vector<stage::hit> *>(userdata);
  hits->emplace_back(to_entity(ud));
  return true;
}

static const scriptable* scriptable_of(entt::registry& registry, entt::entity entity) {
  if (!registry.valid(entity)) [[unlikely]]
    return nullptr;

  const auto* op = registry.try_get<scriptable>(entity);
  if (!op || op->handle == LUA_NOREF) [[unlikely]]
    return nullptr;

  return op;
}

static int world_spawn(lua_State* state) {
  auto* self = static_cast<stage *>(lua_touserdata(state, lua_upvalueindex(1)));
  const std::string_view name = luaL_checkstring(state, 1);
  const std::string_view kind = luaL_checkstring(state, 2);
  const auto x = static_cast<float>(luaL_checknumber(state, 3));
  const auto y = static_cast<float>(luaL_checknumber(state, 4));
  return self->spawn(state, name, kind, x, y);
}

static int world_destroy(lua_State* state) {
  auto* self = static_cast<stage *>(lua_touserdata(state, lua_upvalueindex(1)));
  return self->destroy(state);
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
  const auto* caller = static_cast<scriptable *>(luaL_checkudata(state, 1, "Object"));
  const auto x = static_cast<float>(luaL_checknumber(state, 2));
  const auto y = static_cast<float>(luaL_checknumber(state, 3));
  const auto radius = static_cast<float>(luaL_checknumber(state, 4));
  return self->radar(state, caller->entity, x, y, radius);
}

static int world_raycast(lua_State* state) {
  auto* self = static_cast<stage *>(lua_touserdata(state, lua_upvalueindex(1)));
  const auto* caller = static_cast<scriptable *>(luaL_checkudata(state, 1, "Object"));
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

stage::stage(std::string name)
    : _name(std::move(name)) {
  _registry.on_destroy<scriptable>().connect<&on_scriptable_destroy>();
  _registry.on_destroy<body>().connect<&on_object_destroy>();
  _registry.ctx().emplace<reorder>();

  const auto filename = std::format("stages/{}.lua", _name);
  const auto buffer = io::read(filename);
  const auto chunk = std::format("@{}", filename);
  compile(L, buffer, chunk);

  lua_newtable(L);
  _pool_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_newtable(L);

  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, world_spawn, 1);
  lua_setfield(L, -2, "spawn");

  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, world_destroy, 1);
  lua_setfield(L, -2, "destroy");

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

  _world_ref = luaL_ref(L, LUA_REGISTRYINDEX);

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
  _sleep_margin = largest;
  _wake_margin = largest * .5f;
  _hits.reserve(64);

  lua_getfield(L, -1, "objects");
  if (lua_istable(L, -1)) {
    const auto count = static_cast<int>(lua_objlen(L, -1));

    for (int i = 1; i <= count; ++i) {
      lua_rawgeti(L, -1, i);

      lua_getfield(L, -1, "name");
      const std::string_view label = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
      lua_pop(L, 1);

      lua_getfield(L, -1, "kind");
      const std::string_view kind = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
      lua_pop(L, 1);

      lua_getfield(L, -1, "x");
      const auto ox = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : .0f;
      lua_pop(L, 1);

      lua_getfield(L, -1, "y");
      const auto oy = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : .0f;
      lua_pop(L, 1);

      lua_pop(L, 1);

      spawn(L, label, kind, ox, oy);
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
      const std::string_view label = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
      lua_pop(L, 1);

      lua_getfield(L, -1, "loop");
      const auto loop = lua_isboolean(L, -1) ? lua_toboolean(L, -1) != 0 : false;
      lua_pop(L, 1);

      auto *instance = depot->sound.get(std::format("sounds/{}", label));

      auto **memory = static_cast<class sound **>(lua_newuserdata(L, sizeof(class sound *)));
      *memory = instance;
      luaL_getmetatable(L, "Sound");
      lua_setmetatable(L, -2);

      lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_ref);
      lua_pushvalue(L, -2);
      lua_setfield(L, -2, label.data());
      lua_pop(L, 1);

      _sounds.emplace_back(instance);
      lua_pop(L, 1);

      if (loop)
        instance->set_loop(true);


      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);

  lua_getfield(L, -1, "tilemap");
  if (lua_isstring(L, -1))
    _tilemap = tilemap(lua_tostring(L, -1), _world);
  lua_pop(L, 1);

  lua_getfield(L, -1, "minimap");

  if (lua_istable(L, -1)) [[unlikely]] {
    lua_getfield(L, -1, "solid");
    const auto solid = unpack(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "passable");
    const auto passable = unpack(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "void");
    const auto empty = unpack(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "player");
    const auto player = unpack(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "entity");
    const auto entity = unpack(L, -1);
    lua_pop(L, 1);

    _minimap.emplace(_tilemap, _registry, solid, passable, empty, player, entity);

    auto **userdata = static_cast<minimap **>(lua_newuserdata(L, sizeof(minimap *)));
    *userdata = &*_minimap;
    luaL_getmetatable(L, "Minimap");
    lua_setmetatable(L, -2);

    lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_ref);
    lua_pushvalue(L, -2);
    lua_setfield(L, -2, "minimap");
    lua_pop(L, 1);

    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  lua_getfield(L, -1, "overlay");
  if (lua_istable(L, -1)) {
    lua_getfield(L, -1, "widgets");
    if (lua_isstring(L, -1))
      _overlay = lua_tostring(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "foreground");
    if (lua_isstring(L, -1))
      _foreground = lua_tostring(L, -1);
    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  lua_getfield(L, -1, "particles");
  if (lua_istable(L, -1)) {
    const auto count = static_cast<int>(lua_objlen(L, -1));

    for (int i = 1; i <= count; ++i) {
      lua_rawgeti(L, -1, i);

      lua_getfield(L, -1, "name");
      const std::string_view label = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
      lua_pop(L, 1);

      lua_getfield(L, -1, "kind");
      const std::string_view kind = lua_isstring(L, -1) ? lua_tostring(L, -1) : "";
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

      auto *p = _particlesystem.add(label, kind, px, py, active);

      auto **userdata = static_cast<class particle **>(lua_newuserdata(L, sizeof(class particle *)));
      *userdata = p;
      luaL_getmetatable(L, "Particle");
      lua_setmetatable(L, -2);

      lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_ref);
      lua_pushvalue(L, -2);
      lua_setfield(L, -2, label.data());
      lua_pop(L, 1);

      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);

  _ref = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _ref);

  lua_getfield(L, -1, "on_loop");
  _on_loop = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_camera");
  _on_camera = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_tick");
  _on_tick = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_enter");
  _on_enter = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_leave");
  _on_leave = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_pop(L, 1);
}

stage::~stage() {
  luaL_unref(L, LUA_REGISTRYINDEX, _on_leave);
  _on_leave = LUA_NOREF;

  luaL_unref(L, LUA_REGISTRYINDEX, _on_enter);
  _on_enter = LUA_NOREF;

  luaL_unref(L, LUA_REGISTRYINDEX, _on_tick);
  _on_tick = LUA_NOREF;

  luaL_unref(L, LUA_REGISTRYINDEX, _on_camera);
  _on_camera = LUA_NOREF;

  luaL_unref(L, LUA_REGISTRYINDEX, _on_loop);
  _on_loop = LUA_NOREF;

  luaL_unref(L, LUA_REGISTRYINDEX, _world_ref);
  _world_ref = LUA_NOREF;

  luaL_unref(L, LUA_REGISTRYINDEX, _pool_ref);
  _pool_ref = LUA_NOREF;

  luaL_unref(L, LUA_REGISTRYINDEX, _ref);
  _ref = LUA_NOREF;

  _registry.on_destroy<body>().disconnect<&on_object_destroy>();
  _registry.clear();
  b2DestroyWorld(_world);
}

void stage::update(float delta) {
  {
    for (auto&& [e, tf, an] : _registry.view<sleepable, transform, animation>(entt::exclude<dormant>).each()) {
      if (!culled(tf, an, _sleep_margin))
        continue;

      _registry.emplace<dormant>(e);

      if (auto* b = _registry.try_get<body>(e);
          b && alive(*b))
        b2Body_Disable(b->id);

      const auto& op = _registry.get<scriptable>(e);
      if (op.on_sleep != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, op.on_sleep);
        lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle);
        pcall(L, 1, 0);
      }
    }
  }

  {
    for (auto&& [e, tf, an] : _registry.view<sleepable, dormant, transform, animation>().each()) {
      if (culled(tf, an, _wake_margin))
        continue;

      _registry.remove<dormant>(e);

      if (auto* b = _registry.try_get<body>(e);
          b && alive(*b))
        b2Body_Enable(b->id);

      const auto& op = _registry.get<scriptable>(e);
      if (op.on_wake != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, op.on_wake);
        lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle);
        pcall(L, 1, 0);
      }
    }
  }

  if (_on_loop != LUA_NOREF) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_loop);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _ref);
    lua_pushnumber(L, static_cast<lua_Number>(delta));
    pcall(L, 2, 0);
  }

  for (auto&& [e, op] : _registry.view<scriptable>(entt::exclude<dormant>).each()) {
    if (op.prototype == LUA_NOREF || op.handle == LUA_NOREF) [[unlikely]]
      continue;

    if (op.on_loop != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, op.on_loop);
      lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle);
      lua_pushnumber(L, static_cast<lua_Number>(delta));
      pcall(L, 2, 0);
    }

    if (!_registry.valid(e)) continue;

    auto* a = _registry.try_get<animation>(e);
    if (!a || !a->playing || a->sheet->count == 0) [[unlikely]]
      continue;

    const auto& c = a->sheet->clips[a->active];
    const auto& fr = a->sheet->frames[c.offset + a->current];
    if (c.count == 0 || fr.duration < .0f) [[unlikely]]
      continue;

    a->elapsed += delta;

    if (a->elapsed >= fr.duration) {
      a->elapsed -= fr.duration;
      ++a->current;

      if (a->current >= c.count) {
        a->current = 0;

        if (op.on_animation_end != LUA_NOREF) {
          lua_rawgeti(L, LUA_REGISTRYINDEX, op.on_animation_end);
          lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle);
          lua_rawgeti(L, LUA_REGISTRYINDEX, c.identity.reference);
          pcall(L, 2, 0);
        }

        if (!_registry.valid(e)) continue;

        if (op.on_animation_begin != LUA_NOREF) {
          lua_rawgeti(L, LUA_REGISTRYINDEX, op.on_animation_begin);
          lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle);
          lua_rawgeti(L, LUA_REGISTRYINDEX, c.identity.reference);
          pcall(L, 2, 0);
        }
      }
    }
  }

  _accumulator += delta;

  _interpolation.before = _interpolation.current;

  auto steps = 0;
  while (_accumulator >= _timestep) {
    for (auto&& [en, b, an, tf] :
         _registry.view<body, animation, transform>(entt::exclude<dormant>).each()) {
      if (b.type == body_type::dynamic) {
        tf.previous_x = tf.x;
        tf.previous_y = tf.y;
      }

      const auto& frame = an.sheet->frames[an.sheet->clips[an.active].offset + an.current];

      if (ensure_shape(b, frame, en, tf, _timestep))
        continue;
    }

    b2World_Step(_world, _timestep, _substeps);

    const auto events = b2World_GetBodyEvents(_world);
    auto &rd = _registry.ctx().get<reorder>();

    for (const auto& event : std::span(events.moveEvents, static_cast<size_t>(events.moveCount))) {
      const auto entity = to_entity(event.userData);

      if (!_registry.valid(entity)) [[unlikely]]
        continue;

      const auto* b = _registry.try_get<body>(entity);
      if (!b || b->type != body_type::dynamic) [[unlikely]]
        continue;

      const auto* an = _registry.try_get<animation>(entity);
      if (!an || !an->playing || an->sheet->count == 0) [[unlikely]]
        continue;

      auto& tf = _registry.get<transform>(entity);
      const auto& frame = an->sheet->frames[an->sheet->clips[an->active].offset + an->current];
      const auto position = event.transform.p;
      tf.x = position.x - frame.bound_x - b->extent_x;
      tf.y = position.y - frame.bound_y - b->extent_y;

      auto &r = _registry.get<renderable>(entity);
      const auto z = static_cast<int>(tf.y + frame.height * tf.scale);
      if (r.z != z) [[unlikely]] {
        r.z = z;
        rd.dirty = true;
      }
    }

    _accumulator -= _timestep;
    ++steps;
  }

  if (_on_camera != LUA_NOREF) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_camera);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _ref);

    pcall(L, 1, 2);

    if (lua_isnumber(L, -2))
      _interpolation.current.x = static_cast<float>(lua_tonumber(L, -2));
    if (lua_isnumber(L, -1))
      _interpolation.current.y = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 2);
  }

  if (steps > 0 && _interpolation.ready) {
    const auto inverse = 1.f / static_cast<float>(steps);
    _interpolation.previous.x = _interpolation.current.x
      - (_interpolation.current.x - _interpolation.before.x) * inverse;
    _interpolation.previous.y = _interpolation.current.y
      - (_interpolation.current.y - _interpolation.before.y) * inverse;
  } else {
    _interpolation.previous = _interpolation.current;
    _interpolation.ready = true;
  }

  _interpolation.alpha = _timestep > .0f ? _accumulator / _timestep : .0f;

  static const int _bearings[] = {
    (lua_pushstring(L, "left"), luaL_ref(L, LUA_REGISTRYINDEX)),
    (lua_pushstring(L, "right"), luaL_ref(L, LUA_REGISTRYINDEX)),
    (lua_pushstring(L, "top"), luaL_ref(L, LUA_REGISTRYINDEX)),
    (lua_pushstring(L, "bottom"), luaL_ref(L, LUA_REGISTRYINDEX)),
  };

  const auto vr = viewport.x + viewport.width;
  const auto vb = viewport.y + viewport.height;

  for (auto&& [e, b, bd, op] : _registry.view<const body, boundary, const scriptable>(entt::exclude<dormant>).each()) {
    if (!b2Body_IsValid(b.id) || !b2Shape_IsValid(b.shape)) [[unlikely]]
      continue;

    const auto aabb = b2Shape_GetAABB(b.shape);

    uint8_t current = 0;
    if (aabb.upperBound.x < viewport.x)
      current |= boundary::left;
    if (aabb.lowerBound.x > vr)
      current |= boundary::right;
    if (aabb.upperBound.y < viewport.y)
      current |= boundary::top;
    if (aabb.lowerBound.y > vb)
      current |= boundary::bottom;

    const auto exited  = static_cast<uint8_t>(current & ~bd.previous);
    const auto entered = static_cast<uint8_t>(bd.previous & ~current);

    for (uint8_t bit = 0; bit < 4; ++bit) {
      const auto mask = static_cast<uint8_t>(1u << bit);
      if ((exited & mask) && op.on_screen_exit != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, op.on_screen_exit);
        lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle);
        lua_rawgeti(L, LUA_REGISTRYINDEX, _bearings[bit]);
        pcall(L, 2, 0);

        if (!_registry.valid(e)) break;
      }

      if ((entered & mask) && op.on_screen_enter != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, op.on_screen_enter);
        lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle);
        lua_rawgeti(L, LUA_REGISTRYINDEX, _bearings[bit]);
        pcall(L, 2, 0);

        if (!_registry.valid(e)) break;
      }
    }

    if (!_registry.valid(e)) continue;

    bd.previous = current;
  }

  {
    const auto events = b2World_GetSensorEvents(_world);
    entt::entity ea, eb;

    auto dispatch_sensor = [&](auto *events, size_t count, int scriptable::*callback) {
      for (const auto& event : std::span(events, count)) {
        if (!resolve(event.sensorShapeId, event.visitorShapeId, ea, eb))
          continue;

        const auto *pa = _registry.try_get<scriptable>(ea);
        const auto *pb = _registry.try_get<scriptable>(eb);
        if (pa && pa->*callback != LUA_NOREF)
          dispatch_collision(*pa, pb, pa->*callback);
      }
    };

    dispatch_sensor(events.beginEvents, static_cast<size_t>(events.beginCount), &scriptable::on_collision_begin);
    dispatch_sensor(events.endEvents, static_cast<size_t>(events.endCount), &scriptable::on_collision_end);

    const auto contacts = b2World_GetContactEvents(_world);
    for (const auto& event : std::span(contacts.beginEvents, static_cast<size_t>(contacts.beginCount))) {
      if (!resolve(event.shapeIdA, event.shapeIdB, ea, eb))
        continue;

      auto *pa = _registry.try_get<scriptable>(ea);
      auto *pb = _registry.try_get<scriptable>(eb);
      if (pa) dispatch_collision(*pa, pb, pa->on_collision_begin, &event.manifold.normal);

      pb = _registry.try_get<scriptable>(eb);
      pa = _registry.try_get<scriptable>(ea);
      const auto flipped = b2Vec2{-event.manifold.normal.x, -event.manifold.normal.y};
      if (pb) dispatch_collision(*pb, pa, pb->on_collision_begin, &flipped);
    }

    for (const auto& event : std::span(contacts.endEvents, static_cast<size_t>(contacts.endCount))) {
      if (!resolve(event.shapeIdA, event.shapeIdB, ea, eb))
        continue;

      auto *pa = _registry.try_get<scriptable>(ea);
      auto *pb = _registry.try_get<scriptable>(eb);
      if (pa) dispatch_collision(*pa, pb, pa->on_collision_end);
      pb = _registry.try_get<scriptable>(eb);
      pa = _registry.try_get<scriptable>(ea);
      if (pb) dispatch_collision(*pb, pa, pb->on_collision_end);
    }
  }

  for (auto* sound : _sounds) sound->poll();

  _particlesystem.update(delta);

  auto& rd = _registry.ctx().get<reorder>();
  if (rd.dirty) [[unlikely]] {
    _registry.sort<renderable>([](const renderable& lhs, const renderable& rhs) {
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

  const auto capacity = view.size_hint();
  _vertices.clear();
  _indices.clear();
  _vertices.reserve(capacity * 4);
  _indices.reserve(capacity * 6);

  SDL_Texture *current = nullptr;

  for (auto&& [e, r, a, tf] : view.each()) {
    if (!tf.shown || !a.playing || !a.sheet || a.sheet->count == 0) [[unlikely]]
      continue;

    const auto& c = a.sheet->clips[a.active];
    if (c.count == 0)
      continue;

    const auto& fr = a.sheet->frames[c.offset + a.current];

    const auto rx = tf.previous_x + _interpolation.alpha * (tf.x - tf.previous_x);
    const auto ry = tf.previous_y + _interpolation.alpha * (tf.y - tf.previous_y);

    const auto dw = fr.width * tf.scale;
    const auto dh = fr.height * tf.scale;
    const auto px = rx - viewport.x;
    const auto py = ry - viewport.y;

    if (px + dw < .0f || px > viewport.width ||
        py + dh < .0f || py > viewport.height)
      continue;

    auto *texture = static_cast<SDL_Texture *>(*a.sheet->pixmap);

    if (texture != current) [[unlikely]]
      submit(std::exchange(current, texture), _vertices, _indices);

    auto u0 = fr.u0;
    auto v0 = fr.v0;
    auto u1 = fr.u1;
    auto v1 = fr.v1;

    if (std::to_underlying(tf.flip) & SDL_FLIP_HORIZONTAL) std::swap(u0, u1);
    if (std::to_underlying(tf.flip) & SDL_FLIP_VERTICAL) std::swap(v0, v1);

    const auto alpha = std::clamp(tf.alpha, .0f, 255.f) / 255.f;

    const auto hw = dw * .5f;
    const auto hh = dh * .5f;
    const auto mx = px + hw;
    const auto my = py + hh;

    auto sa = .0f, ca = 1.f;
    if (tf.angle != .0f) [[unlikely]]
      sincos(to_radians(tf.angle), sa, ca);

    const auto dx0 = -hw * ca + hh * sa;
    const auto dy0 = -hw * sa - hh * ca;
    const auto dx1 =  hw * ca + hh * sa;
    const auto dy1 =  hw * sa - hh * ca;

    const auto base = static_cast<int>(_vertices.size());
    const SDL_FColor color{1.f, 1.f, 1.f, alpha};

    _vertices.emplace_back(SDL_Vertex{{mx + dx0, my + dy0}, color, {u0, v0}});
    _vertices.emplace_back(SDL_Vertex{{mx + dx1, my + dy1}, color, {u1, v0}});
    _vertices.emplace_back(SDL_Vertex{{mx - dx0, my - dy0}, color, {u1, v1}});
    _vertices.emplace_back(SDL_Vertex{{mx - dx1, my - dy1}, color, {u0, v1}});

    _indices.emplace_back(base);
    _indices.emplace_back(base + 1);
    _indices.emplace_back(base + 2);
    _indices.emplace_back(base);
    _indices.emplace_back(base + 2);
    _indices.emplace_back(base + 3);
  }

  submit(current, _vertices, _indices);

  _particlesystem.draw();

  _tilemap.draw_foreground();

  if (_minimap)
    _minimap->draw();

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
  if (_on_enter == LUA_NOREF)
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, _on_enter);
  lua_rawgeti(L, LUA_REGISTRYINDEX, _ref);
  pcall(L, 1, 0);
}

void stage::on_leave() {
  if (_on_leave != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_leave);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _ref);
    pcall(L, 1, 0);
  }

  conceal();
}

void stage::expose() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_ref);
  lua_setglobal(L, "pool");
  lua_rawgeti(L, LUA_REGISTRYINDEX, _world_ref);
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
  lua_rawgeti(L, LUA_REGISTRYINDEX, _ref);
  lua_pushinteger(L, static_cast<lua_Integer>(tick));
  pcall(L, 2, 0);
}

int stage::spawn(lua_State* state, std::string_view name, std::string_view kind, float x, float y) {
  const auto entity = _registry.create();

  _registry.emplace<renderable>(entity);

  auto& tf = _registry.emplace<transform>(entity);
  tf.previous_x = tf.x = x;
  tf.previous_y = tf.y = y;

  auto& op = _registry.emplace<scriptable>(entity);
  op.registry = &_registry;
  op.entity = entity;
  op.name = entt::hashed_string{name.data()};
  op.kind = entt::hashed_string{kind.data()};
  object::bind(op, name, kind);
  const auto prototype = op.prototype;
  const auto handle = op.handle;
  const auto on_spawn = op.on_spawn;

  depot->string.get(name);
  depot->string.get(kind);

  lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);
  lua_getfield(L, -1, "animation");

  if (lua_istable(L, -1)) {
    const auto* sheet = depot->spritesheet.get(kind, L, -1);

    auto& a = _registry.emplace<animation>(entity);
    a.sheet = sheet;
    a.active = sheet->initial;
    a.playing = sheet->count > 0;

    if (a.playing) {
      const auto& fr = sheet->frames[sheet->clips[a.active].offset];
      const auto& tf2 = _registry.get<transform>(entity);
      _registry.get<renderable>(entity).z = static_cast<int>(tf2.y + fr.height * tf2.scale);
      _registry.ctx().get<reorder>().dirty = true;
    }

    if (sheet->collidable) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);
      lua_getfield(L, -1, "body");
      const std::string_view behavior = lua_isstring(L, -1) ? lua_tostring(L, -1) : "kinematic";
      lua_pop(L, 2);

      const auto [type, b2type] = mapping(behavior.data());

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
    const auto drowsy = lua_isboolean(L, -1) ? lua_toboolean(L, -1) != 0 : false;
    lua_pop(L, 1);
    lua_pop(L, 1);

    if (drowsy)
      _registry.emplace<sleepable>(entity);
  }

  if (on_spawn != LUA_NOREF) [[unlikely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, on_spawn);
    lua_rawgeti(L, LUA_REGISTRYINDEX, handle);
    pcall(L, 1, 0);
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_ref);
  lua_rawgeti(L, LUA_REGISTRYINDEX, handle);
  lua_setfield(L, -2, name.data());
  lua_pop(L, 1);

  if (auto [a, t] = _registry.try_get<animation, transform>(entity);
      a && t && _registry.all_of<sleepable>(entity)) {
    if (culled(*t, *a, _sleep_margin)) {
      _registry.emplace<dormant>(entity);

      if (auto* b = _registry.try_get<body>(entity);
          b && alive(*b))
        b2Body_Disable(b->id);
    }
  }

  lua_rawgeti(state, LUA_REGISTRYINDEX, handle);
  return 1;
}

int stage::destroy(lua_State* state) {
  auto* proxy = static_cast<scriptable *>(luaL_checkudata(state, 1, "Object"));
  if (!_registry.valid(proxy->entity))
    return 0;

  const auto* label = depot->string.get(proxy->name);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_ref);
  lua_pushnil(L);
  lua_setfield(L, -2, label);
  lua_pop(L, 1);

  _registry.destroy(proxy->entity);
  return 0;
}

int stage::count(lua_State *state) {
  const auto x = static_cast<float>(luaL_checknumber(state, 1));
  const auto y = static_cast<float>(luaL_checknumber(state, 2));
  const auto w = static_cast<float>(luaL_checknumber(state, 3));
  const auto h = static_cast<float>(luaL_checknumber(state, 4));
  const auto kind = entt::hashed_string{luaL_checkstring(state, 5)};

  const auto aabb = b2AABB{{x, y}, {x + w, y + h}};

  _hits.clear();
  b2World_OverlapAABB(_world, aabb, filter, +gather, &_hits);

  int total = 0;
  for (const auto& [entity, fraction] : _hits) {
    const auto* op = scriptable_of(_registry, entity);
    if (op && op->kind == kind)
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
  const auto kind = entt::hashed_string{luaL_checkstring(state, 5)};

  const b2AABB aabb = {{x, y}, {x + w, y + h}};

  _hits.clear();
  b2World_OverlapAABB(_world, aabb, filter, +gather, &_hits);

  lua_createtable(state, static_cast<int>(_hits.size()), 0);
  int index = 1;

  for (const auto& [entity, fraction] : _hits) {
    const auto* op = scriptable_of(_registry, entity);
    if (!op || op->kind != kind)
      continue;

    lua_rawgeti(state, LUA_REGISTRYINDEX, op->handle);
    lua_rawseti(state, -2, index++);
  }

  return 1;
}

int stage::radar(lua_State *state, entt::entity caller, float x, float y, float radius) {
  const b2Vec2 center{x, y};
  const auto proxy = b2MakeProxy(&center, 1, radius);

  _hits.clear();
  b2World_OverlapShape(
    _world,
    &proxy,
    filter,
    +gather,
    &_hits
  );

  lua_createtable(state, static_cast<int>(_hits.size()), 0);
  int index = 1;

  for (const auto& [entity, fraction] : _hits) {
    if (entity == caller)
      continue;

    const auto* op = scriptable_of(_registry, entity);
    if (!op)
      continue;

    lua_rawgeti(state, LUA_REGISTRYINDEX, op->handle);
    lua_rawseti(state, -2, index++);
  }

  return 1;
}

int stage::raycast(lua_State* state, entt::entity caller, float x, float y, float angle, float distance) {
  const auto radians = to_radians(angle);
  auto sine = .0f, cosine = .0f;
  sincos(radians, sine, cosine);
  const b2Vec2 origin{x, y};
  const b2Vec2 translation{cosine * distance, sine * distance};

  _hits.clear();
  b2World_CastRay(
    _world,
    origin,
    translation,
    filter,
    +[](b2ShapeId shape, b2Vec2, b2Vec2, float fraction, void *userdata) -> float {
      const auto *ud = b2Shape_GetUserData(shape);
      if (!ud) [[unlikely]]
        return 1.f;

      auto *hits = static_cast<std::vector<hit> *>(userdata);
      hits->emplace_back(to_entity(ud), fraction);
      return 1.f;
    },
    &_hits
  );

  std::ranges::sort(_hits, {}, &hit::fraction);

  lua_createtable(state, static_cast<int>(_hits.size()), 0);
  int index = 1;

  for (const auto& [entity, fraction] : _hits) {
    if (entity == caller)
      continue;

    const auto* op = scriptable_of(_registry, entity);
    if (!op)
      continue;

    lua_rawgeti(state, LUA_REGISTRYINDEX, op->handle);
    lua_rawseti(state, -2, index++);
  }

  return 1;
}

int stage::pathfind(lua_State* state, float x1, float y1, float x2, float y2, float radius) {
  return _tilemap.pathfind(state, x1, y1, x2, y2, radius);
}

void stage::dispatch_collision(const scriptable& self, const scriptable* target, int callback_ref, const b2Vec2* normal) {
  if (callback_ref == LUA_NOREF) [[likely]]
    return;

  if (self.handle == LUA_NOREF || !target) [[unlikely]]
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, callback_ref);
  lua_rawgeti(L, LUA_REGISTRYINDEX, self.handle);
  lua_rawgeti(L, LUA_REGISTRYINDEX, target->name_ref);
  lua_rawgeti(L, LUA_REGISTRYINDEX, target->kind_ref);
  if (normal) {
    lua_pushnumber(L, static_cast<lua_Number>(normal->x));
    lua_pushnumber(L, static_cast<lua_Number>(normal->y));
    pcall(L, 5, 0);
  } else {
    pcall(L, 3, 0);
  }
}
