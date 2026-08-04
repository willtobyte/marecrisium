namespace {
  namespace lookup {
    constexpr auto dynamic_bodytype = "dynamic"_hs;
    constexpr auto static_bodytype = "static"_hs;
  }

  constexpr auto picks = 16uz;
  constexpr auto corners = 4uz;
  constexpr auto elements = 6uz;
  constexpr std::array pattern{0, 1, 2, 0, 2, 3};

static bool on_event(void *userdata, SDL_Event *event) {
  if (event->type != SDL_EVENT_TEXT_INPUT) [[likely]]
    return true;

  static_cast<stage *>(userdata)->on_text(event->text.text);
  return true;
}

static void* encode(entt::entity e) {
  return reinterpret_cast<void*>(static_cast<uintptr_t>(e) + 1);
}

static entt::entity decode(const void* p) {
  return static_cast<entt::entity>(reinterpret_cast<uintptr_t>(p) - 1);
}

static void flush(SDL_Texture* texture, const SDL_Vertex* first, const SDL_Vertex* last, const int* indices) {
  if (first == last) [[unlikely]]
    return;

  const auto count = static_cast<size_t>(last - first);
  SDL_RenderGeometry(
    renderer,
    texture,
    first,
    static_cast<int>(count),
    indices,
    static_cast<int>(count / corners * elements));
}

static constexpr b2QueryFilter filter{
  B2_DEFAULT_CATEGORY_BITS,
  B2_DEFAULT_MASK_BITS,
};

static color read_color(lua_State *state, int index) {
  lua_rawgeti(state, index, 1);
  const auto r = static_cast<uint8_t>(std::clamp(lua_tonumber(state, -1), lua_Number{}, lua_Number{255}));
  lua_pop(state, 1);

  lua_rawgeti(state, index, 2);
  const auto g = static_cast<uint8_t>(std::clamp(lua_tonumber(state, -1), lua_Number{}, lua_Number{255}));
  lua_pop(state, 1);

  lua_rawgeti(state, index, 3);
  const auto b = static_cast<uint8_t>(std::clamp(lua_tonumber(state, -1), lua_Number{}, lua_Number{255}));
  lua_pop(state, 1);

  return {r, g, b};
}

static bool outside(const transform &tf, const animation &an, float margin) {
  const auto &frame = an.sheet->frames[an.sheet->clips[an.active].offset + an.current];
  const auto width = frame.width * tf.scale;
  const auto height = frame.height * tf.scale;
  const auto sx = tf.x - viewport.x;
  const auto sy = tf.y - viewport.y;

  return
    sx + width  < -margin ||
    sx >  viewport.width  + margin ||
    sy + height < -margin ||
    sy >  viewport.height + margin;
}

static constexpr auto body_types(const char *s) -> std::pair<body_type, b2BodyType> {
  const auto id = entt::hashed_string{s};
  switch (id) {
    case lookup::dynamic_bodytype: return {body_type::dynamic, b2_dynamicBody};
    case lookup::static_bodytype: return {body_type::stationary, b2_staticBody};
    default: return {body_type::kinematic, b2_kinematicBody};
  }
}

static void sync_body(body& b, const frame& frame, entt::entity entity, const transform& tf, float timestep) {
  auto created = false;
  if (b.snapshot != &frame) [[unlikely]] {
    b.snapshot = &frame;
    b.origin_x = frame.collider.offset_x;
    b.origin_y = frame.collider.offset_y;
    const auto hx = frame.collider.width * .5f;
    const auto hy = frame.collider.height * .5f;

    if (B2_IS_NULL(b.shape)) {
      const auto polygon = b2MakeBox(hx, hy);

      auto sdef = b2DefaultShapeDef();
      sdef.userData = encode(entity);
      sdef.enableContactEvents = b.events;
      sdef.enableSensorEvents = b.events;

      if (b.type == body_type::dynamic) {
        sdef.density = 1.f;
        sdef.material.friction = .0f;
      }

      b.shape = b2CreatePolygonShape(b.id, &sdef, &polygon);
      b.extent_x = hx;
      b.extent_y = hy;

      if (b.type != body_type::kinematic) {
        b2Body_SetTransform(b.id, center_of(b, tf, &frame), b2Rot_identity);
        return;
      }

      created = true;
    }

    if (hx != b.extent_x || hy != b.extent_y) [[unlikely]] {
      const auto polygon = b2MakeBox(hx, hy);
      b2Shape_SetPolygon(b.shape, &polygon);
      b.extent_x = hx;
      b.extent_y = hy;
    }
  }

  if (b.type != body_type::kinematic)
    return;

  const auto center = center_of(b, tf, &frame);
  if (created || center.x != b.target_x || center.y != b.target_y) [[unlikely]] {
    b.target_x = center.x;
    b.target_y = center.y;
    b2Body_SetTargetTransform(b.id, {center, b2Rot_identity}, timestep);
    b.moving = true;
    return;
  }

  if (!b.moving) [[likely]]
    return;

  const auto position = b2Body_GetPosition(b.id);
  if (position.x == center.x && position.y == center.y) {
    b2Body_SetLinearVelocity(b.id, b2Vec2_zero);
    b.moving = false;
    return;
  }

  b2Body_SetTargetTransform(b.id, {center, b2Rot_identity}, timestep);
}

static bool resolve_entities(b2ShapeId a, b2ShapeId b, entt::entity &ea, entt::entity &eb) {
  if (!b2Shape_IsValid(a) || !b2Shape_IsValid(b)) [[unlikely]]
    return false;

  const auto *ua = b2Shape_GetUserData(a);
  const auto *ub = b2Shape_GetUserData(b);
  if (!ua || !ub) [[unlikely]]
    return false;

  ea = decode(ua);
  eb = decode(ub);
  return true;
}

static void release_scriptable(entt::registry& registry, entt::entity entity) {
  auto& op = registry.get<scriptable>(entity);

  lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle);
  auto* handle = static_cast<proxy*>(luaL_testudata(L, -1, "Object"));
  if (handle)
    handle->registry = nullptr;
  lua_pop(L, 1);

  luaL_unref(L, LUA_REGISTRYINDEX, op.label);
  luaL_unref(L, LUA_REGISTRYINDEX, op.handle);
}

static void destroy_body(entt::registry& registry, entt::entity entity) {
  auto& b = registry.get<body>(entity);
  if (alive(b))
    b2DestroyBody(b.id);
}

static bool collect_hits(b2ShapeId shape, void *userdata) {
  const auto *ud = b2Shape_GetUserData(shape);
  if (!ud) [[unlikely]]
    return true;

  auto *hits = static_cast<std::vector<entt::entity> *>(userdata);
  hits->emplace_back(decode(ud));
  return true;
}

static const scriptable* find_scriptable(entt::registry& registry, entt::entity entity) {
  if (!registry.valid(entity)) [[unlikely]]
    return nullptr;

  const auto* op = registry.try_get<scriptable>(entity);
  if (!op || op->handle == LUA_NOREF) [[unlikely]]
    return nullptr;

  return op;
}

static stage* owner(lua_State* state) {
  auto** slot = static_cast<stage**>(lua_touserdata(state, lua_upvalueindex(1)));
  if (!slot || !*slot) {
    luaL_error(state, "stage is no longer alive");
    std::unreachable();
  }

  return *slot;
}

template<typename T>
static void invalidate(lua_State* state, int table, const char* name) {
  const auto index = table < 0 ? lua_gettop(state) + table + 1 : table;
  lua_pushnil(state);
  while (lua_next(state, index)) {
    auto** value = static_cast<T**>(luaL_testudata(state, -1, name));
    if (value)
      *value = nullptr;
    lua_pop(state, 1);
  }
}

static int spawn_callback(lua_State* state) {
  auto* self = owner(state);
  const std::string_view name = luaL_checkstring(state, 1);
  const std::string_view kind = luaL_checkstring(state, 2);
  const auto x = static_cast<float>(luaL_checknumber(state, 3));
  const auto y = static_cast<float>(luaL_checknumber(state, 4));
  return self->spawn(state, name, kind, x, y);
}

static int destroy_callback(lua_State* state) {
  return owner(state)->destroy(state);
}

static int count_callback(lua_State* state) {
  return owner(state)->count(state);
}

static int find_callback(lua_State* state) {
  return owner(state)->find(state);
}

static int radar_callback(lua_State* state) {
  auto* self = owner(state);
  const auto* caller = static_cast<proxy *>(luaL_checkudata(state, 1, "Object"));
  const auto x = static_cast<float>(luaL_checknumber(state, 2));
  const auto y = static_cast<float>(luaL_checknumber(state, 3));
  const auto radius = static_cast<float>(luaL_checknumber(state, 4));
  return self->radar(state, caller->entity, x, y, radius);
}

static int raycast_callback(lua_State* state) {
  auto* self = owner(state);
  const auto* caller = static_cast<proxy *>(luaL_checkudata(state, 1, "Object"));
  const auto x = static_cast<float>(luaL_checknumber(state, 2));
  const auto y = static_cast<float>(luaL_checknumber(state, 3));
  const auto angle = static_cast<float>(luaL_checknumber(state, 4));
  const auto distance = static_cast<float>(luaL_checknumber(state, 5));
  return self->raycast(state, caller->entity, x, y, angle, distance);
}

template<void (stage::*callback)(float, float, const char*)>
static void dispatch_button(stage& self, uint32_t buttons, float x, float y) {
  const auto active = buttons & (SDL_BUTTON_LMASK | SDL_BUTTON_MMASK | SDL_BUTTON_RMASK);
  if (!active) [[likely]]
    return;

  static constexpr const char* names[] = {"left", "middle", "right"};
  (self.*callback)(x, y, names[static_cast<size_t>(std::countr_zero(active))]);
}

template<typename T>
static void dispatch_sensors(
    stage& self,
    entt::registry& registry,
    const T* entries,
    size_t count,
    int prototype::*callback) {
  entt::entity ea, eb;
  for (const auto& event : std::span(entries, count)) {
    if (!resolve_entities(event.sensorShapeId, event.visitorShapeId, ea, eb))
      continue;

    const auto* pa = registry.try_get<scriptable>(ea);
    const auto* pb = registry.try_get<scriptable>(eb);
    if (pa && pa->blueprint->*callback != LUA_NOREF)
      self.dispatch_collision(*pa, pb, pa->blueprint->*callback);
  }
}

static bool by_depth(const renderable& lhs, const renderable& rhs) {
  return lhs.z < rhs.z;
}

}

stage::stage(std::string name)
    : _name(std::move(name)) {
  const timer::scope scope{_timer};

  _registry.on_destroy<scriptable>().connect<&release_scriptable>();
  _registry.on_destroy<body>().connect<&destroy_body>();
  _registry.ctx().emplace<reorder>();
  _registry.ctx().emplace<dormancy>();

  _vertices.reserve(4096);
  _indices.reserve(6144);

  const auto chunk = std::format("@stages/{}.lua", _name);
  const auto path = std::string_view{chunk}.substr(1);
  const auto source = io::read(path);

  if (luaL_loadbuffer(L, reinterpret_cast<const char*>(source.data()), source.size(), chunk.c_str()) != LUA_OK) [[unlikely]] {
    lua_error(L);
    std::unreachable();
  }

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

  lua_getfield(L, -1, "substeps");
  if (lua_isnumber(L, -1))
    _substeps = static_cast<int>(lua_tointeger(L, -1));
  lua_pop(L, 1);

  b2WorldDef def = b2DefaultWorldDef();
  def.gravity = gravity;
  _physics = b2CreateWorld(&def);

  lua_newtable(L);
  _pool = luaL_ref(L, LUA_REGISTRYINDEX);

  auto** owner = static_cast<stage**>(lua_newuserdata(L, sizeof(stage*)));
  *owner = this;
  _owner = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_newtable(L);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _owner);
  lua_pushcclosure(L, spawn_callback, 1);
  lua_setfield(L, -2, "spawn");

  lua_rawgeti(L, LUA_REGISTRYINDEX, _owner);
  lua_pushcclosure(L, destroy_callback, 1);
  lua_setfield(L, -2, "destroy");

  lua_rawgeti(L, LUA_REGISTRYINDEX, _owner);
  lua_pushcclosure(L, count_callback, 1);
  lua_setfield(L, -2, "count");

  lua_rawgeti(L, LUA_REGISTRYINDEX, _owner);
  lua_pushcclosure(L, find_callback, 1);
  lua_setfield(L, -2, "find");

  lua_rawgeti(L, LUA_REGISTRYINDEX, _owner);
  lua_pushcclosure(L, radar_callback, 1);
  lua_setfield(L, -2, "radar");

  lua_rawgeti(L, LUA_REGISTRYINDEX, _owner);
  lua_pushcclosure(L, raycast_callback, 1);
  lua_setfield(L, -2, "raycast");

  _world = luaL_ref(L, LUA_REGISTRYINDEX);

  const auto largest = std::max(viewport.width, viewport.height);
  _sleep_margin = largest;
  _wake_margin = largest * .5f;
  _hits.reserve(8);

  lua_getfield(L, -1, "objects");
  if (lua_istable(L, -1)) {
    const auto count = static_cast<int>(lua_objlen(L, -1));

    for (int i = 1; i <= count; ++i) {
      lua_rawgeti(L, -1, i);

      lua_getfield(L, -1, "name");
      const std::string label{lua_isstring(L, -1) ? lua_tostring(L, -1) : ""};
      lua_pop(L, 1);

      lua_getfield(L, -1, "kind");
      const std::string kind{lua_isstring(L, -1) ? lua_tostring(L, -1) : ""};
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

  _pending.reserve(_registry.storage<sleepable>().size());

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
      const std::string label{lua_isstring(L, -1) ? lua_tostring(L, -1) : ""};
      lua_pop(L, 1);

      lua_getfield(L, -1, "loop");
      const auto loop = lua_isboolean(L, -1) ? lua_toboolean(L, -1) != 0 : false;
      lua_pop(L, 1);

      const auto key = std::format("sounds/{}", label);
      auto *instance = depot->sound.get(key);
      auto **memory = static_cast<class sound **>(lua_newuserdata(L, sizeof(class sound *)));
      *memory = instance;
      luaL_getmetatable(L, "Sound");
      lua_setmetatable(L, -2);

      lua_rawgeti(L, LUA_REGISTRYINDEX, _pool);
      lua_pushvalue(L, -2);
      lua_setfield(L, -2, label.c_str());
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
    _tilemap = tilemap(lua_tostring(L, -1), _physics);
  lua_pop(L, 1);

  lua_getfield(L, -1, "minimap");

  if (lua_istable(L, -1)) [[unlikely]] {
    lua_getfield(L, -1, "solid");
    const auto solid = read_color(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "passable");
    const auto passable = read_color(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "void");
    const auto empty = read_color(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "player");
    const auto player = read_color(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, -1, "entity");
    const auto entity = read_color(L, -1);
    lua_pop(L, 1);

    _minimap.emplace(_tilemap, _registry, solid, passable, empty, player, entity);

    auto **userdata = static_cast<minimap **>(lua_newuserdata(L, sizeof(minimap *)));
    *userdata = &*_minimap;
    luaL_getmetatable(L, "Minimap");
    lua_setmetatable(L, -2);

    lua_rawgeti(L, LUA_REGISTRYINDEX, _pool);
    lua_pushvalue(L, -2);
    lua_setfield(L, -2, "minimap");
    lua_pop(L, 1);

    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  lua_getfield(L, -1, "foregrounds");
  if (lua_istable(L, -1)) {
    const auto count = static_cast<int>(lua_objlen(L, -1));
    _foregrounds.reserve(static_cast<std::size_t>(count));

    for (int i = 1; i <= count; ++i) {
      lua_rawgeti(L, -1, i);
      if (lua_isstring(L, -1))
        _foregrounds.emplace_back(lua_tostring(L, -1));
      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);

  lua_getfield(L, -1, "particles");
  if (lua_istable(L, -1)) {
    const auto count = static_cast<int>(lua_objlen(L, -1));

    for (int i = 1; i <= count; ++i) {
      lua_rawgeti(L, -1, i);

      lua_getfield(L, -1, "name");
      const std::string label{lua_isstring(L, -1) ? lua_tostring(L, -1) : ""};
      lua_pop(L, 1);

      lua_getfield(L, -1, "kind");
      const std::string kind{lua_isstring(L, -1) ? lua_tostring(L, -1) : ""};
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

      auto *particle = _particlesystem.add(label, kind, px, py, active);

      auto **userdata = static_cast<class particle **>(lua_newuserdata(L, sizeof(class particle *)));
      *userdata = particle;
      luaL_getmetatable(L, "Particle");
      lua_setmetatable(L, -2);

      lua_rawgeti(L, LUA_REGISTRYINDEX, _pool);
      lua_pushvalue(L, -2);
      lua_setfield(L, -2, label.c_str());
      lua_pop(L, 1);

      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);

  _table = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _table);

  lua_getfield(L, -1, "on_loop");
  _on_loop = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_camera");
  _on_camera = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_text");
  _on_text = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_enter");
  _on_enter = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_leave");
  _on_leave = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_press");
  _on_press = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_release");
  _on_release = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_pop(L, 1);
}

stage::~stage() {
  SDL_RemoveEventWatch(on_event, this);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _owner);
  auto** owner = static_cast<stage**>(lua_touserdata(L, -1));
  *owner = nullptr;
  lua_pop(L, 1);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _pool);
  invalidate<minimap>(L, -1, "Minimap");
  invalidate<particle>(L, -1, "Particle");
  lua_pop(L, 1);

  luaL_unref(L, LUA_REGISTRYINDEX, _on_release);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_press);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_leave);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_enter);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_text);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_camera);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_loop);
  luaL_unref(L, LUA_REGISTRYINDEX, _world);
  luaL_unref(L, LUA_REGISTRYINDEX, _owner);
  luaL_unref(L, LUA_REGISTRYINDEX, _pool);
  luaL_unref(L, LUA_REGISTRYINDEX, _table);

  _registry.on_destroy<body>().disconnect<&destroy_body>();
  _registry.clear();
  b2DestroyWorld(_physics);
}

void stage::update(float delta) {
  _pending.clear();
  for (auto&& [e, tf, an] : _registry.view<sleepable, transform, animation>(entt::exclude<dormant>).each()) {
    if (outside(tf, an, _sleep_margin))
      _pending.emplace_back(e);
  }

  for (const auto e : _pending) {
    if (!_registry.valid(e)) [[unlikely]]
      continue;

    _registry.emplace<dormant>(e);

    if (auto* b = _registry.try_get<body>(e);
        b && alive(*b))
      b2Body_Disable(b->id);

    const auto& op = _registry.get<scriptable>(e);
    if (op.blueprint->on_sleep != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, op.blueprint->on_sleep);
      lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle);
      {
        const auto base = lua_gettop(L) - 1;
        lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
        lua_insert(L, base);
        const auto status = lua_pcall(L, 1, 0, base);
        lua_remove(L, base);
        if (status != LUA_OK) [[unlikely]] {
          lua_error(L);
          std::unreachable();
        }
      }
    }
  }

  auto& state = _registry.ctx().get<dormancy>();
  if (state.dirty || state.viewport != viewport) [[unlikely]] {
    state.viewport = viewport;
    state.dirty = false;

    _pending.clear();
    for (auto&& [e, tf, an] : _registry.view<sleepable, dormant, transform, animation>().each()) {
      if (!outside(tf, an, _wake_margin))
        _pending.emplace_back(e);
    }

    for (const auto e : _pending) {
      if (!_registry.valid(e)) [[unlikely]]
        continue;

      _registry.remove<dormant>(e);

      if (auto* b = _registry.try_get<body>(e);
          b && alive(*b))
        b2Body_Enable(b->id);

      const auto& op = _registry.get<scriptable>(e);
      if (op.blueprint->on_wake != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, op.blueprint->on_wake);
        lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle);
        {
          const auto base = lua_gettop(L) - 1;
          lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
          lua_insert(L, base);
          const auto status = lua_pcall(L, 1, 0, base);
          lua_remove(L, base);
          if (status != LUA_OK) [[unlikely]] {
            lua_error(L);
            std::unreachable();
          }
        }
      }
    }
  }

  float mx, my;
  const auto buttons = SDL_GetMouseState(&mx, &my);
  SDL_RenderCoordinatesFromWindow(renderer, mx, my, &mx, &my);
  mx += viewport.x;
  my += viewport.y;

  const auto pressed = buttons & ~_mouse_previous_buttons;
  const auto released = _mouse_previous_buttons & ~buttons;
  _mouse_previous_buttons = buttons;

  dispatch_button<&stage::dispatch_press>(*this, pressed, mx, my);
  dispatch_button<&stage::dispatch_release>(*this, released, mx, my);

  dispatch_hover(mx, my);

  if (_on_loop != LUA_NOREF) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_loop);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
    lua_pushnumber(L, static_cast<lua_Number>(delta));
    {
      const auto base = lua_gettop(L) - 2;
      lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
      lua_insert(L, base);
      const auto status = lua_pcall(L, 2, 0, base);
      lua_remove(L, base);
      if (status != LUA_OK) [[unlikely]] {
        lua_error(L);
        std::unreachable();
      }
    }
  }

  for (auto&& [e, op] : _registry.view<scriptable>(entt::exclude<dormant>).each()) {
    if (!op.blueprint || op.blueprint->table == LUA_NOREF || op.handle == LUA_NOREF) [[unlikely]]
      continue;

    const auto& bp = *op.blueprint;
    if (bp.on_loop != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, bp.on_loop);
      lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle);
      lua_pushnumber(L, static_cast<lua_Number>(delta));
      {
        const auto base = lua_gettop(L) - 2;
        lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
        lua_insert(L, base);
        const auto status = lua_pcall(L, 2, 0, base);
        lua_remove(L, base);
        if (status != LUA_OK) [[unlikely]] {
          lua_error(L);
          std::unreachable();
        }
      }

      if (!_registry.valid(e))
        continue;
    }

    auto* a = _registry.try_get<animation>(e);
    if (!a || !a->playing || a->sheet->count == 0) [[unlikely]]
      continue;

    const auto& clip = a->sheet->clips[a->active];
    const auto& frame = a->sheet->frames[clip.offset + a->current];
    if (clip.count == 0 || frame.duration <= .0f) [[unlikely]]
      continue;

    a->elapsed += delta;
    if (a->elapsed < frame.duration) [[likely]]
      continue;

    a->elapsed -= frame.duration;
    if (++a->current < clip.count)
      continue;

    a->current = 0;

    if (bp.on_animation_end != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, bp.on_animation_end);
      lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle);
      lua_rawgeti(L, LUA_REGISTRYINDEX, clip.identity.name);
      {
        const auto base = lua_gettop(L) - 2;
        lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
        lua_insert(L, base);
        const auto status = lua_pcall(L, 2, 0, base);
        lua_remove(L, base);
        if (status != LUA_OK) [[unlikely]] {
          lua_error(L);
          std::unreachable();
        }
      }

      if (!_registry.valid(e))
        continue;
    }

    if (bp.on_animation_begin != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, bp.on_animation_begin);
      lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle);
      lua_rawgeti(L, LUA_REGISTRYINDEX, clip.identity.name);
      {
        const auto base = lua_gettop(L) - 2;
        lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
        lua_insert(L, base);
        const auto status = lua_pcall(L, 2, 0, base);
        lua_remove(L, base);
        if (status != LUA_OK) [[unlikely]] {
          lua_error(L);
          std::unreachable();
        }
      }
    }
  }

  auto& rd = _registry.ctx().get<reorder>();
  _accumulator += delta;

  _interpolation.before = _interpolation.current;

  for (auto&& [en, b, tf] : _registry.view<body, transform>().each()) {
    if (!b.dirty) [[unlikely]]
      continue;
    b.dirty = false;
    const auto* an = _registry.try_get<animation>(en);
    const frame* frame = nullptr;
    if (an && an->playing && an->sheet->count > 0) [[likely]] {
      frame = &an->sheet->frames[an->sheet->clips[an->active].offset + an->current];
      if (B2_IS_NULL(b.shape)) [[unlikely]]
        sync_body(b, *frame, en, tf, _timestep);
    }
    const auto center = center_of(b, tf, frame);
    b2Body_SetTransform(b.id, center, b2Rot_identity);
  }

  auto steps = 0;
  while (_accumulator >= _timestep) {
    for (auto&& [en, b, tf] :
         _registry.view<body, transform>(entt::exclude<dormant>).each()) {
      if (b.type == body_type::dynamic) {
        tf.previous_x = tf.x;
        tf.previous_y = tf.y;
        continue;
      }

      if (b.type == body_type::kinematic) {
        const auto center = b2Vec2{tf.x + b.origin_x + b.extent_x, tf.y + b.origin_y + b.extent_y};
        if (center.x != b.target_x || center.y != b.target_y) [[unlikely]] {
          b.target_x = center.x;
          b.target_y = center.y;
          b2Body_SetTargetTransform(b.id, {center, b2Rot_identity}, _timestep);
          b.moving = true;
          continue;
        }
        if (!b.moving) [[likely]]
          continue;
        const auto position = b2Body_GetPosition(b.id);
        if (position.x == center.x && position.y == center.y) {
          b2Body_SetLinearVelocity(b.id, b2Vec2_zero);
          b.moving = false;
        }
      }
    }

    b2World_Step(_physics, _timestep, _substeps);

    const auto events = b2World_GetBodyEvents(_physics);

    for (const auto& event : std::span(events.moveEvents, static_cast<size_t>(events.moveCount))) {
      const auto entity = decode(event.userData);

      if (!_registry.valid(entity)) [[unlikely]]
        continue;

      const auto* b = _registry.try_get<body>(entity);
      const auto* an = _registry.try_get<animation>(entity);
      if (!b || b->type != body_type::dynamic || !an || !an->playing || an->sheet->count == 0) [[unlikely]]
        continue;

      auto& tf = _registry.get<transform>(entity);
      const auto& frame = an->sheet->frames[an->sheet->clips[an->active].offset + an->current];
      const auto position = event.transform.p;
      tf.x = position.x - frame.collider.offset_x - b->extent_x;
      tf.y = position.y - frame.collider.offset_y - b->extent_y;

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
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);

    {
      const auto base = lua_gettop(L) - 1;
      lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
      lua_insert(L, base);
      const auto status = lua_pcall(L, 1, 2, base);
      lua_remove(L, base);
      if (status != LUA_OK) [[unlikely]] {
        lua_error(L);
        std::unreachable();
      }
    }

    if (lua_isnumber(L, -2))
      _interpolation.current.x = static_cast<float>(lua_tonumber(L, -2));
    if (lua_isnumber(L, -1))
      _interpolation.current.y = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 2);
  }

  const auto inverse = steps > 0 && _interpolation.ready
    ? 1.f / static_cast<float>(steps)
    : .0f;
  _interpolation.previous.x = _interpolation.current.x
    - (_interpolation.current.x - _interpolation.before.x) * inverse;
  _interpolation.previous.y = _interpolation.current.y
    - (_interpolation.current.y - _interpolation.before.y) * inverse;
  _interpolation.ready = true;
  _interpolation.alpha = _timestep > .0f ? _accumulator / _timestep : .0f;

  static const int bearings[] = {
    (lua_pushstring(L, "left"), luaL_ref(L, LUA_REGISTRYINDEX)),
    (lua_pushstring(L, "right"), luaL_ref(L, LUA_REGISTRYINDEX)),
    (lua_pushstring(L, "top"), luaL_ref(L, LUA_REGISTRYINDEX)),
    (lua_pushstring(L, "bottom"), luaL_ref(L, LUA_REGISTRYINDEX)),
  };

  const auto vr = viewport.x + viewport.width;
  const auto vb = viewport.y + viewport.height;

  auto screen = _registry.view<const body, boundary, const scriptable>(entt::exclude<dormant>);
  screen.use<boundary>();

  for (auto&& [e, b, bd, op] : screen.each()) {
    if (!b2Body_IsValid(b.id) || !b2Shape_IsValid(b.shape)) [[unlikely]]
      continue;

    const auto aabb = b2Shape_GetAABB(b.shape);

    const auto current = static_cast<uint8_t>(
      (aabb.upperBound.x < viewport.x ? boundary::left : 0)
      | (aabb.lowerBound.x > vr ? boundary::right : 0)
      | (aabb.upperBound.y < viewport.y ? boundary::top : 0)
      | (aabb.lowerBound.y > vb ? boundary::bottom : 0));
    const auto exited = static_cast<uint8_t>(current & ~bd.previous);
    const auto entered = static_cast<uint8_t>(bd.previous & ~current);
    bd.previous = current;

    for (auto bits = static_cast<uint8_t>(exited | entered);
         bits;
         bits &= static_cast<uint8_t>(bits - 1)) {
      const auto bit = static_cast<size_t>(std::countr_zero(bits));
      const auto mask = static_cast<uint8_t>(1u << bit);
      const auto callback = (exited & mask)
        ? op.blueprint->on_screen_exit
        : op.blueprint->on_screen_enter;
      if (callback == LUA_NOREF)
        continue;

      lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
      lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle);
      lua_rawgeti(L, LUA_REGISTRYINDEX, bearings[bit]);
      {
        const auto base = lua_gettop(L) - 2;
        lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
        lua_insert(L, base);
        const auto status = lua_pcall(L, 2, 0, base);
        lua_remove(L, base);
        if (status != LUA_OK) [[unlikely]] {
          lua_error(L);
          std::unreachable();
        }
      }

      if (!_registry.valid(e))
        break;
    }
  }

  {
    const auto sensors = b2World_GetSensorEvents(_physics);
    dispatch_sensors(*this, _registry, sensors.beginEvents, static_cast<size_t>(sensors.beginCount), &prototype::on_collision_begin);
    dispatch_sensors(*this, _registry, sensors.endEvents, static_cast<size_t>(sensors.endCount), &prototype::on_collision_end);

    entt::entity ea, eb;
    const auto contacts = b2World_GetContactEvents(_physics);
    for (const auto& event : std::span(contacts.beginEvents, static_cast<size_t>(contacts.beginCount))) {
      if (!resolve_entities(event.shapeIdA, event.shapeIdB, ea, eb))
        continue;

      auto *pa = _registry.try_get<scriptable>(ea);
      auto *pb = _registry.try_get<scriptable>(eb);
      if (pa) dispatch_collision(*pa, pb, pa->blueprint->on_collision_begin, &event.manifold.normal);

      pb = _registry.try_get<scriptable>(eb);
      pa = _registry.try_get<scriptable>(ea);
      const auto flipped = b2Vec2{-event.manifold.normal.x, -event.manifold.normal.y};
      if (pb) dispatch_collision(*pb, pa, pb->blueprint->on_collision_begin, &flipped);
    }

    for (const auto& event : std::span(contacts.endEvents, static_cast<size_t>(contacts.endCount))) {
      if (!resolve_entities(event.shapeIdA, event.shapeIdB, ea, eb))
        continue;

      auto *pa = _registry.try_get<scriptable>(ea);
      auto *pb = _registry.try_get<scriptable>(eb);
      if (pa) dispatch_collision(*pa, pb, pa->blueprint->on_collision_end);
      pb = _registry.try_get<scriptable>(eb);
      pa = _registry.try_get<scriptable>(ea);
      if (pb) dispatch_collision(*pb, pa, pb->blueprint->on_collision_end);
    }
  }

  for (auto* sound : _sounds) sound->poll();

  _particlesystem.update(delta);

  if (rd.dirty) [[unlikely]] {
    _registry.sort<renderable>(by_depth, entt::insertion_sort{});
    rd.dirty = false;
  }
}

void stage::draw() {
  const auto cx = _interpolation.previous.x + _interpolation.alpha * (_interpolation.current.x - _interpolation.previous.x);
  const auto cy = _interpolation.previous.y + _interpolation.alpha * (_interpolation.current.y - _interpolation.previous.y);
  viewport.x = std::floor(cx * viewport.scale) / viewport.scale;
  viewport.y = std::floor(cy * viewport.scale) / viewport.scale;

  _tilemap.draw_background();

  auto view = _registry.view<const renderable, const animation, transform>(entt::exclude<dormant>);
  view.use<renderable>();

  const auto capacity = view.size_hint();
  const auto vcount = capacity * corners;
  if (_vertices.size() < vcount)
    _vertices.resize(vcount);

  const auto icount = capacity * elements;
  if (_indices.size() < icount) {
    const auto first = _indices.size() / elements;
    _indices.resize(icount);
    auto* output = _indices.data() + first * elements;

    for (auto i = first; i < capacity; ++i) {
      const auto base = static_cast<int>(i * corners);
      for (const auto offset : pattern)
        *output++ = base + offset;
    }
  }

  auto* output = _vertices.data();
  auto* batch = output;
  const pixmap* source = nullptr;
  SDL_Texture* current = nullptr;

  for (auto&& [e, r, a, tf] : view.each()) {
    if (!tf.shown || !a.playing || !a.sheet || a.sheet->count == 0) [[unlikely]]
      continue;

    const auto& clip = a.sheet->clips[a.active];
    if (clip.count == 0)
      continue;

    const auto& frame = a.sheet->frames[clip.offset + a.current];

    const auto rx = tf.previous_x + _interpolation.alpha * (tf.x - tf.previous_x);
    const auto ry = tf.previous_y + _interpolation.alpha * (tf.y - tf.previous_y);

    const auto dw = frame.width * tf.scale;
    const auto dh = frame.height * tf.scale;
    auto px = rx - viewport.x;
    auto py = ry - viewport.y;
    const auto moved = tf.moved;
    if (moved) [[unlikely]]
      tf.moved = false;
    if (moved || tf.previous_x != tf.x || tf.previous_y != tf.y) [[unlikely]] {
      px = std::floor(px);
      py = std::floor(py);
    }

    if (px + dw < .0f || px > viewport.width ||
        py + dh < .0f || py > viewport.height)
      continue;

    if (a.sheet->pixmap != source) [[unlikely]] {
      source = a.sheet->pixmap;
      auto *texture = static_cast<SDL_Texture *>(*source);

      if (texture != current) {
        flush(current, batch, output, _indices.data());
        current = texture;
        batch = output;
      }
    }

    auto u0 = frame.u0;
    auto v0 = frame.v0;
    auto u1 = frame.u1;
    auto v1 = frame.v1;

    if (std::to_underlying(tf.flip) & SDL_FLIP_HORIZONTAL) std::swap(u0, u1);
    if (std::to_underlying(tf.flip) & SDL_FLIP_VERTICAL) std::swap(v0, v1);

    const auto alpha = std::clamp(tf.alpha, .0f, 255.f) / 255.f;

    const auto hw = dw * .5f;
    const auto hh = dh * .5f;
    const auto mx = px + hw;
    const auto my = py + hh;

    auto sine = .0f;
    auto cosine = 1.f;
    if (tf.angle != .0f) [[unlikely]] {
      const auto radians = tf.angle * (std::numbers::pi_v<float> / 180.f);
      sine = std::sin(radians);
      cosine = std::cos(radians);
    }

    const auto dx0 = -hw * cosine + hh * sine;
    const auto dy0 = -hw * sine - hh * cosine;
    const auto dx1 = hw * cosine + hh * sine;
    const auto dy1 = hw * sine - hh * cosine;

    const SDL_FColor color{1.f, 1.f, 1.f, alpha};

    *output++ = SDL_Vertex{{mx + dx0, my + dy0}, color, {u0, v0}};
    *output++ = SDL_Vertex{{mx + dx1, my + dy1}, color, {u1, v0}};
    *output++ = SDL_Vertex{{mx - dx0, my - dy0}, color, {u1, v1}};
    *output++ = SDL_Vertex{{mx - dx1, my - dy1}, color, {u0, v1}};
  }

  flush(current, batch, output, _indices.data());

  _particlesystem.draw();

  _tilemap.draw_foreground();

  if (_minimap)
    _minimap->draw();

#ifdef DEBUG
  SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);

  const auto aabb = b2AABB{{viewport.x, viewport.y}, {viewport.x + viewport.width, viewport.y + viewport.height}};

  b2World_OverlapAABB(_physics, aabb, filter, +[](b2ShapeId shape, void*) -> bool {
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

  SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

  if (_origin.x != _target.x || _origin.y != _target.y) {
    SDL_RenderLine(
      renderer,
      _origin.x - viewport.x,
      _origin.y - viewport.y,
      _target.x - viewport.x,
      _target.y - viewport.y
    );
  }

  if (_radius > .0f) {
    static constexpr size_t segments = 32;
    static const auto unit = []() {
      std::array<SDL_FPoint, segments + 1> table{};
      for (size_t i = 0; i <= segments; ++i) {
        const auto angle = static_cast<float>(i) * (2.f * std::numbers::pi_v<float> / static_cast<float>(segments));
        table[i] = {std::cos(angle), std::sin(angle)};
      }
      return table;
    }();

    static SDL_FPoint points[segments + 1];
    for (size_t i = 0; i <= segments; ++i) {
      points[i].x = _center.x + unit[i].x * _radius - viewport.x;
      points[i].y = _center.y + unit[i].y * _radius - viewport.y;
    }

    SDL_RenderLines(renderer, points, static_cast<int>(segments + 1));
  }

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
#endif
}

void stage::on_enter() {
  if (_on_text != LUA_NOREF) {
    SDL_AddEventWatch(on_event, this);

    const auto properties = SDL_CreateProperties();
    SDL_SetBooleanProperty(properties, SDL_PROP_TEXTINPUT_AUTOCORRECT_BOOLEAN, false);
    SDL_StartTextInputWithProperties(SDL_GetRenderWindow(renderer), properties);
    SDL_DestroyProperties(properties);
  }

  if (_on_enter == LUA_NOREF)
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, _on_enter);
  lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
  {
    const auto base = lua_gettop(L) - 1;
    lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
    lua_insert(L, base);
    const auto status = lua_pcall(L, 1, 0, base);
    lua_remove(L, base);
    if (status != LUA_OK) [[unlikely]] {
      lua_error(L);
      std::unreachable();
    }
  }
}

void stage::on_leave() {
  SDL_RemoveEventWatch(on_event, this);
  SDL_StopTextInput(SDL_GetRenderWindow(renderer));

  if (_on_leave != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_leave);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
    {
      const auto base = lua_gettop(L) - 1;
      lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
      lua_insert(L, base);
      const auto status = lua_pcall(L, 1, 0, base);
      lua_remove(L, base);
      if (status != LUA_OK) [[unlikely]] {
        lua_error(L);
        std::unreachable();
      }
    }
  }

  conceal();
}

void stage::expose() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _pool);
  lua_setglobal(L, "pool");
  lua_rawgeti(L, LUA_REGISTRYINDEX, _world);
  lua_setglobal(L, "world");
}

void stage::conceal() {
  lua_pushnil(L);
  lua_setglobal(L, "pool");
  lua_pushnil(L);
  lua_setglobal(L, "world");
}

void stage::on_text(std::string_view text) {
  if (_on_text == LUA_NOREF) [[unlikely]]
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, _on_text);
  lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
  lua_pushlstring(L, text.data(), text.size());
  {
    const auto base = lua_gettop(L) - 2;
    lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
    lua_insert(L, base);
    const auto status = lua_pcall(L, 2, 0, base);
    lua_remove(L, base);
    if (status != LUA_OK) [[unlikely]] {
      lua_error(L);
      std::unreachable();
    }
  }
}

int stage::spawn(lua_State* state, std::string_view name, std::string_view kind, float x, float y) {
  const auto entity = _registry.create();

  _registry.emplace<renderable>(entity);

  auto& tf = _registry.emplace<transform>(entity);
  tf.previous_x = tf.x = x;
  tf.previous_y = tf.y = y;

  auto& op = _registry.emplace<scriptable>(entity);
  op.name = entt::hashed_string{name.data(), name.size()};
  op.kind = entt::hashed_string{kind.data(), kind.size()};
  object::bind(_registry, entity, op, name, kind);
  const auto prototype = op.blueprint->table;
  const auto handle = op.handle;
  const auto on_spawn = op.blueprint->on_spawn;

  lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);
  lua_getfield(L, -1, "animation");

  if (lua_istable(L, -1)) {
    const auto* sheet = depot->spritesheet.get(kind, L, -1);

    auto& a = _registry.emplace<animation>(entity);
    a.sheet = sheet;
    a.active = sheet->initial;
    a.playing = sheet->count > 0;

    if (a.playing) {
      const auto& frame = sheet->frames[sheet->clips[a.active].offset];
      const auto& tf2 = _registry.get<transform>(entity);
      _registry.get<renderable>(entity).z = static_cast<int>(tf2.y + frame.height * tf2.scale);
      _registry.ctx().get<reorder>().dirty = true;
    }

    if (sheet->collidable) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);
      lua_getfield(L, -1, "body");
      const auto* behavior = lua_isstring(L, -1) ? lua_tostring(L, -1) : "kinematic";
      const auto [type, b2type] = body_types(behavior);
      lua_pop(L, 2);

      b2BodyDef bdef = b2DefaultBodyDef();
      bdef.userData = encode(entity);
      bdef.type = b2type;

      if (type == body_type::dynamic) {
        bdef.isBullet = true;
        bdef.fixedRotation = true;
      }

      const auto id = b2CreateBody(_physics, &bdef);
      const auto events = op.blueprint->on_collision_begin != LUA_NOREF || op.blueprint->on_collision_end != LUA_NOREF;
      _registry.emplace<body>(entity, id, b2_nullShapeId, .0f, .0f, .0f, .0f, type, events, false, true);

      if (op.blueprint->on_screen_exit != LUA_NOREF || op.blueprint->on_screen_enter != LUA_NOREF)
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
    {
      const auto base = lua_gettop(L) - 1;
      lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
      lua_insert(L, base);
      const auto status = lua_pcall(L, 1, 0, base);
      lua_remove(L, base);
      if (status != LUA_OK) [[unlikely]] {
        _registry.destroy(entity);
        return lua_error(L);
      }
    }
  }

  if (!_registry.valid(entity)) [[unlikely]] {
    lua_pushnil(state);
    return 1;
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, _pool);
  lua_rawgeti(L, LUA_REGISTRYINDEX, handle);
  lua_setfield(L, -2, name.data());
  lua_pop(L, 1);

  if (auto [a, t] = _registry.try_get<animation, transform>(entity);
      a && t && _registry.all_of<sleepable>(entity)) {
    if (outside(*t, *a, _sleep_margin)) {
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
  auto* self = static_cast<proxy *>(luaL_checkudata(state, 1, "Object"));
  if (self->registry != &_registry || !_registry.valid(self->entity)) [[unlikely]]
    return 0;

  const auto& op = _registry.get<scriptable>(self->entity);

  lua_rawgeti(L, LUA_REGISTRYINDEX, op.label);
  const auto* label = lua_tostring(L, -1);
  lua_rawgeti(L, LUA_REGISTRYINDEX, _pool);
  lua_pushnil(L);
  lua_setfield(L, -2, label);
  lua_pop(L, 2);

  _registry.destroy(self->entity);
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
  b2World_OverlapAABB(_physics, aabb, filter, +collect_hits, &_hits);

  int total = 0;
  for (const auto entity : _hits) {
    const auto* op = find_scriptable(_registry, entity);
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
  b2World_OverlapAABB(_physics, aabb, filter, +collect_hits, &_hits);

  lua_createtable(state, static_cast<int>(_hits.size()), 0);
  int index = 1;

  for (const auto entity : _hits) {
    const auto* op = find_scriptable(_registry, entity);
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

#ifdef DEBUG
  _center = center;
  _radius = radius;
#endif

  _hits.clear();
  b2World_OverlapShape(
    _physics,
    &proxy,
    filter,
    +collect_hits,
    &_hits
  );

  lua_createtable(state, static_cast<int>(_hits.size()), 0);
  int index = 1;

  for (const auto entity : _hits) {
    if (entity == caller)
      continue;

    const auto* op = find_scriptable(_registry, entity);
    if (!op)
      continue;

    lua_rawgeti(state, LUA_REGISTRYINDEX, op->handle);
    lua_rawseti(state, -2, index++);
  }

  return 1;
}

int stage::raycast(lua_State* state, entt::entity caller, float x, float y, float angle, float distance) {
  const auto radians = angle * (std::numbers::pi_v<float> / 180.f);
  const auto sine = std::sin(radians);
  const auto cosine = std::cos(radians);
  const b2Vec2 origin{x, y};
  const b2Vec2 translation{cosine * distance, sine * distance};

#ifdef DEBUG
  _origin = origin;
  _target = {origin.x + translation.x, origin.y + translation.y};
#endif

  struct result final {
    entt::registry* registry;
    entt::entity caller;
    int target{LUA_NOREF};
    float fraction{1.f};
    bool wall{};
  } query{&_registry, caller};

  b2World_CastRay(
    _physics,
    origin,
    translation,
    filter,
    +[](b2ShapeId shape, b2Vec2, b2Vec2, float fraction, void* userdata) -> float {
      auto* value = static_cast<result*>(userdata);
      const auto* data = b2Shape_GetUserData(shape);
      if (!data) [[unlikely]] {
        if (fraction <= value->fraction) {
          value->target = LUA_NOREF;
          value->fraction = fraction;
          value->wall = true;
        }

        return value->fraction;
      }

      const auto entity = decode(data);
      if (entity == value->caller) [[unlikely]]
        return -1.f;

      const auto* object = find_scriptable(*value->registry, entity);
      if (!object) [[unlikely]]
        return -1.f;

      if (fraction < value->fraction ||
          (fraction == value->fraction && !value->wall && value->target == LUA_NOREF)) {
        value->target = object->handle;
        value->fraction = fraction;
        value->wall = false;
      }

      return value->fraction;
    },
    &query
  );

  query.target == LUA_NOREF
    ? static_cast<void>(lua_pushnil(state))
    : static_cast<void>(lua_rawgeti(state, LUA_REGISTRYINDEX, query.target));

  return 1;
}

void stage::dispatch_collision(const scriptable& self, const scriptable* target, int callback, const b2Vec2* normal) {
  if (callback == LUA_NOREF) [[likely]]
    return;

  if (self.handle == LUA_NOREF || !target) [[unlikely]]
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
  lua_rawgeti(L, LUA_REGISTRYINDEX, self.handle);
  lua_rawgeti(L, LUA_REGISTRYINDEX, target->label);
  lua_rawgeti(L, LUA_REGISTRYINDEX, target->blueprint->kind);
  if (!normal) {
    {
      const auto base = lua_gettop(L) - 3;
      lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
      lua_insert(L, base);
      const auto status = lua_pcall(L, 3, 0, base);
      lua_remove(L, base);
      if (status != LUA_OK) [[unlikely]] {
        lua_error(L);
        std::unreachable();
      }
    }
    return;
  }

  lua_pushnumber(L, static_cast<lua_Number>(normal->x));
  lua_pushnumber(L, static_cast<lua_Number>(normal->y));
  {
    const auto base = lua_gettop(L) - 5;
    lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
    lua_insert(L, base);
    const auto status = lua_pcall(L, 5, 0, base);
    lua_remove(L, base);
    if (status != LUA_OK) [[unlikely]] {
      lua_error(L);
      std::unreachable();
    }
  }
}

uint8_t stage::pick_at(float x, float y, entt::entity* buffer, uint8_t capacity) const noexcept {
  constexpr auto half = .5f;
  const b2AABB aabb = {{x - half, y - half}, {x + half, y + half}};

  struct context final {
    entt::entity* hits;
    uint8_t capacity;
    uint8_t count;
  };

  context ctx{buffer, capacity, 0};

  b2World_OverlapAABB(
    _physics, aabb, filter,
    [](b2ShapeId shape, void* userdata) -> bool {
      auto* value = static_cast<context*>(userdata);
      if (value->count >= value->capacity) [[unlikely]]
        return false;

      const auto* ud = b2Shape_GetUserData(shape);
      if (!ud) [[unlikely]]
        return true;

      value->hits[value->count++] = decode(ud);
      return true;
    },
    &ctx);

  return ctx.count;
}

entt::entity stage::find_topmost(std::span<const entt::entity> hits) const noexcept {
  if (hits.empty()) [[unlikely]]
    return entt::null;

  entt::entity topmost = entt::null;
  auto depth = std::numeric_limits<int>::min();
  auto rank = std::numeric_limits<std::size_t>::max();
  const auto* order = _registry.storage<renderable>();

  for (const auto entity : hits) {
    if (!_registry.valid(entity) || _registry.all_of<dormant>(entity)) [[unlikely]]
      continue;

    const auto [an, tf, r] = _registry.try_get<animation, transform, renderable>(entity);
    if (!an || !tf || !r || !tf->shown || tf->alpha <= .0f) [[unlikely]]
      continue;

    const auto current = order->index(entity);
    if (r->z > depth || (r->z == depth && current < rank)) {
      topmost = entity;
      depth = r->z;
      rank = current;
    }
  }

  return topmost;
}

void stage::dispatch_miss(int callback, float x, float y, const char* button) {
  if (callback == LUA_NOREF) [[likely]]
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
  lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
  lua_pushnumber(L, static_cast<lua_Number>(x));
  lua_pushnumber(L, static_cast<lua_Number>(y));
  lua_pushstring(L, button);
  {
    const auto base = lua_gettop(L) - 4;
    lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
    lua_insert(L, base);
    const auto status = lua_pcall(L, 4, 0, base);
    lua_remove(L, base);
    if (status != LUA_OK) [[unlikely]] {
      lua_error(L);
      std::unreachable();
    }
  }
}

void stage::dispatch_press(float x, float y, const char* button) {
  std::array<entt::entity, picks> buffer{};
  const auto count = pick_at(x, y, buffer.data(), static_cast<uint8_t>(buffer.size()));

  if (count == 0) [[likely]] {
    dispatch_miss(_on_press, x, y, button);
    return;
  }

  const auto topmost = find_topmost(std::span(buffer.data(), count));
  if (topmost == entt::null) [[unlikely]]
    return;

  const auto* proxy = _registry.try_get<scriptable>(topmost);
  if (!proxy || proxy->handle == LUA_NOREF || proxy->blueprint->on_press == LUA_NOREF) [[unlikely]]
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy->blueprint->on_press);
  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy->handle);
  lua_pushnumber(L, static_cast<lua_Number>(x));
  lua_pushnumber(L, static_cast<lua_Number>(y));
  lua_pushstring(L, button);
  {
    const auto base = lua_gettop(L) - 4;
    lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
    lua_insert(L, base);
    const auto status = lua_pcall(L, 4, 0, base);
    lua_remove(L, base);
    if (status != LUA_OK) [[unlikely]] {
      lua_error(L);
      std::unreachable();
    }
  }
}

void stage::dispatch_release(float x, float y, const char* button) {
  std::array<entt::entity, picks> buffer{};
  const auto count = pick_at(x, y, buffer.data(), static_cast<uint8_t>(buffer.size()));

  if (count == 0) [[likely]] {
    dispatch_miss(_on_release, x, y, button);
    return;
  }

  const auto topmost = find_topmost(std::span(buffer.data(), count));
  if (topmost == entt::null) [[unlikely]]
    return;

  const auto* proxy = _registry.try_get<scriptable>(topmost);
  if (!proxy || proxy->handle == LUA_NOREF || proxy->blueprint->on_release == LUA_NOREF) [[unlikely]]
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy->blueprint->on_release);
  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy->handle);
  lua_pushnumber(L, static_cast<lua_Number>(x));
  lua_pushnumber(L, static_cast<lua_Number>(y));
  lua_pushstring(L, button);
  {
    const auto base = lua_gettop(L) - 4;
    lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
    lua_insert(L, base);
    const auto status = lua_pcall(L, 4, 0, base);
    lua_remove(L, base);
    if (status != LUA_OK) [[unlikely]] {
      lua_error(L);
      std::unreachable();
    }
  }
}

void stage::dispatch_hover(float x, float y) {
  std::array<entt::entity, picks> buffer{};
  const auto count = pick_at(x, y, buffer.data(), static_cast<uint8_t>(buffer.size()));
  auto topmost = find_topmost(std::span(buffer.data(), count));

  if (topmost == _hovered) [[likely]]
    return;

  dispatch_unhover();

  const auto refreshed = pick_at(x, y, buffer.data(), static_cast<uint8_t>(buffer.size()));
  topmost = find_topmost(std::span(buffer.data(), refreshed));
  if (topmost == entt::null) [[likely]]
    return;

  _hovered = topmost;

  const auto* proxy = _registry.try_get<scriptable>(topmost);
  if (!proxy || proxy->handle == LUA_NOREF || proxy->blueprint->on_hover == LUA_NOREF)
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy->blueprint->on_hover);
  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy->handle);
  {
    const auto base = lua_gettop(L) - 1;
    lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
    lua_insert(L, base);
    const auto status = lua_pcall(L, 1, 0, base);
    lua_remove(L, base);
    if (status != LUA_OK) [[unlikely]] {
      lua_error(L);
      std::unreachable();
    }
  }
}

void stage::dispatch_unhover() {
  if (_hovered == entt::null) [[likely]]
    return;

  const auto entity = std::exchange(_hovered, entt::null);
  if (!_registry.valid(entity)) [[unlikely]]
    return;

  const auto* proxy = _registry.try_get<scriptable>(entity);
  if (!proxy || proxy->handle == LUA_NOREF || proxy->blueprint->on_unhover == LUA_NOREF)
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy->blueprint->on_unhover);
  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy->handle);
  {
    const auto base = lua_gettop(L) - 1;
    lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
    lua_insert(L, base);
    const auto status = lua_pcall(L, 1, 0, base);
    lua_remove(L, base);
    if (status != LUA_OK) [[unlikely]] {
      lua_error(L);
      std::unreachable();
    }
  }
}
