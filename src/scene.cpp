namespace {
  constexpr auto picks = 16uz;

static void* encode(entt::entity e) {
  return reinterpret_cast<void*>(static_cast<uintptr_t>(e) + 1);
}

static entt::entity decode(const void* p) {
  return static_cast<entt::entity>(reinterpret_cast<uintptr_t>(p) - 1);
}

static constexpr b2QueryFilter filter{
  B2_DEFAULT_CATEGORY_BITS,
  B2_DEFAULT_MASK_BITS,
};

static void sync_body(body& b, const frame& frame, entt::entity entity, const transform& tf) {
  const auto hx = frame.collider.width * .5f * tf.scale;
  const auto hy = frame.collider.height * .5f * tf.scale;
  b.origin_x = frame.collider.offset_x * tf.scale;
  b.origin_y = frame.collider.offset_y * tf.scale;

  if (B2_IS_NULL(b.shape)) [[unlikely]] {
    auto sdef = b2DefaultShapeDef();
    sdef.userData = encode(entity);
    sdef.enableContactEvents = false;
    sdef.enableSensorEvents = false;
    const auto polygon = b2MakeBox(hx, hy);
    b.shape = b2CreatePolygonShape(b.id, &sdef, &polygon);
  } else if (hx != b.extent_x || hy != b.extent_y) [[unlikely]] {
    const auto polygon = b2MakeBox(hx, hy);
    b2Shape_SetPolygon(b.shape, &polygon);
  }

  b.extent_x = hx;
  b.extent_y = hy;
  b2Body_SetTransform(b.id, center_of(b, tf), b2Rot_identity);
}

static void release_scriptable(entt::registry& registry, entt::entity entity) {
  auto& op = registry.get<scriptable>(entity);

  lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle_ref);
  auto* handle = static_cast<proxy*>(luaL_testudata(L, -1, "Object"));
  if (handle)
    handle->registry = nullptr;
  lua_pop(L, 1);

  luaL_unref(L, LUA_REGISTRYINDEX, op.label_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, op.handle_ref);
}

static void destroy_body(entt::registry& registry, entt::entity entity) {
  b2DestroyBody(registry.get<body>(entity).id);
}

template<void (scene::*callback)(float, float, const char*)>
static void dispatch_button(scene& self, uint32_t buttons, float x, float y) {
  const auto active = buttons & (SDL_BUTTON_LMASK | SDL_BUTTON_MMASK | SDL_BUTTON_RMASK);
  if (!active) [[likely]]
    return;

  static constexpr const char* names[] = {"left", "middle", "right"};
  (self.*callback)(x, y, names[static_cast<size_t>(std::countr_zero(active))]);
}

static bool by_depth(const renderable& lhs, const renderable& rhs) {
  return lhs.z < rhs.z;
}

}

scene::scene(std::string name)
    : _name(std::move(name)) {
  const timer::scope scope{_timer};

  _registry.on_destroy<scriptable>().connect<&release_scriptable>();
  _registry.on_destroy<body>().connect<&destroy_body>();
  _registry.ctx().emplace<reorder>();

  const auto chunk = std::format("@scenes/{}.lua", _name);
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

  b2WorldDef def = b2DefaultWorldDef();
  _world = b2CreateWorld(&def);

  lua_newtable(L);
  _pool_ref = luaL_ref(L, LUA_REGISTRYINDEX);

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

      spawn(label, kind, ox, oy);
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

      lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_ref);
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

  _table_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _table_ref);

  lua_getfield(L, -1, "on_loop");
  _on_loop_ref = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_camera");
  _on_camera_ref = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_enter");
  _on_enter_ref = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_leave");
  _on_leave_ref = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_press");
  _on_press_ref = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_release");
  _on_release_ref = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_pop(L, 1);
}

scene::~scene() {
  luaL_unref(L, LUA_REGISTRYINDEX, _on_release_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_press_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_leave_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_enter_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_camera_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_loop_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, _pool_ref);
  luaL_unref(L, LUA_REGISTRYINDEX, _table_ref);

  _registry.on_destroy<body>().disconnect<&destroy_body>();
  _registry.clear();
  b2DestroyWorld(_world);
}

void scene::update(float delta) {
  for (auto&& [en, b, tf] : _registry.view<body, transform>().each()) {
    if (!b.dirty) [[likely]]
      continue;

    const auto* an = _registry.try_get<animation>(en);
    if (!an || !an->playing || an->sheet->count == 0) [[unlikely]]
      continue;

    b.dirty = false;
    sync_body(b, an->sheet->frames[an->sheet->clips[an->active].offset + an->current], en, tf);
  }

  float mx, my;
  const auto buttons = SDL_GetMouseState(&mx, &my);
  SDL_RenderCoordinatesFromWindow(renderer, mx, my, &mx, &my);
  mx += viewport.x;
  my += viewport.y;

  const auto pressed = buttons & ~_mouse_previous_buttons;
  const auto released = _mouse_previous_buttons & ~buttons;
  _mouse_previous_buttons = buttons;

  dispatch_button<&scene::dispatch_press>(*this, pressed, mx, my);
  dispatch_button<&scene::dispatch_release>(*this, released, mx, my);

  dispatch_hover(mx, my);

  if (_on_loop_ref != LUA_NOREF) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_loop_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table_ref);
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

  for (auto&& [e, op] : _registry.view<scriptable>().each()) {
    if (!op.blueprint || op.blueprint->table_ref == LUA_NOREF || op.handle_ref == LUA_NOREF) [[unlikely]]
      continue;

    const auto& bp = *op.blueprint;
    if (bp.on_loop_ref != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, bp.on_loop_ref);
      lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle_ref);
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

    if (auto* b = _registry.try_get<body>(e); b) [[unlikely]]
      b->dirty = true;

    if (++a->current < clip.count)
      continue;

    a->current = 0;

    if (bp.on_animation_end_ref != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, bp.on_animation_end_ref);
      lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle_ref);
      lua_rawgeti(L, LUA_REGISTRYINDEX, clip.identity.name_ref);
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

    if (bp.on_animation_begin_ref != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, bp.on_animation_begin_ref);
      lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle_ref);
      lua_rawgeti(L, LUA_REGISTRYINDEX, clip.identity.name_ref);
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

  if (_on_camera_ref != LUA_NOREF) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_camera_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table_ref);

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
      viewport.x = std::floor(static_cast<float>(lua_tonumber(L, -2)) * viewport.scale) / viewport.scale;
    if (lua_isnumber(L, -1))
      viewport.y = std::floor(static_cast<float>(lua_tonumber(L, -1)) * viewport.scale) / viewport.scale;
    lua_pop(L, 2);
  }

  for (auto* sound : _sounds) sound->poll();

  if (rd.dirty) [[unlikely]] {
    _registry.sort<renderable>(by_depth, entt::insertion_sort{});
    rd.dirty = false;
  }
}

void scene::draw() {
  auto view = _registry.view<const renderable, const animation, const transform>();
  view.use<renderable>();

  for (auto&& [e, r, a, tf] : view.each()) {
    if (!tf.shown) [[unlikely]]
      continue;

    const auto& clip = a.sheet->clips[a.active];
    const auto& frame = a.sheet->frames[clip.offset + a.current];
    const auto* sheet = a.sheet->pixmap;

    sheet->draw(
      frame.u0 * static_cast<float>(sheet->width()),
      frame.v0 * static_cast<float>(sheet->height()),
      frame.width,
      frame.height,
      std::floor(tf.x - viewport.x),
      std::floor(tf.y - viewport.y),
      frame.width * tf.scale,
      frame.height * tf.scale,
      tf.angle,
      static_cast<uint8_t>(std::clamp(tf.alpha, .0f, 255.f)),
      tf.flip);
  }

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

void scene::on_enter() {
  if (_on_enter_ref == LUA_NOREF)
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, _on_enter_ref);
  lua_rawgeti(L, LUA_REGISTRYINDEX, _table_ref);
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

void scene::on_leave() {
  if (_on_leave_ref != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_leave_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table_ref);
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

void scene::expose() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_ref);
  lua_setglobal(L, "pool");
}

void scene::conceal() {
  lua_pushnil(L);
  lua_setglobal(L, "pool");
}

void scene::spawn(std::string_view name, std::string_view kind, float x, float y) {
  const auto entity = _registry.create();

  _registry.emplace<renderable>(entity, static_cast<int>(_registry.storage<renderable>().size()));

  auto& tf = _registry.emplace<transform>(entity);
  tf.x = x;
  tf.y = y;

  auto& op = _registry.emplace<scriptable>(entity);
  op.name = entt::hashed_string{name.data(), name.size()};
  op.kind = entt::hashed_string{kind.data(), kind.size()};
  object::bind(_registry, entity, op, name, kind);
  const auto prototype = op.blueprint->table_ref;
  const auto handle = op.handle_ref;
  const auto on_spawn = op.blueprint->on_spawn_ref;

  lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);
  lua_getfield(L, -1, "animation");

  if (lua_istable(L, -1)) {
    const auto* sheet = depot->spritesheet.get(kind, L, -1);

    auto& a = _registry.emplace<animation>(entity);
    a.sheet = sheet;
    a.active = sheet->initial;
    a.playing = sheet->count > 0;

    if (sheet->collidable) {
      b2BodyDef bdef = b2DefaultBodyDef();
      bdef.userData = encode(entity);
      bdef.type = b2_kinematicBody;

      const auto id = b2CreateBody(_world, &bdef);
      _registry.emplace<body>(entity, id, b2_nullShapeId, .0f, .0f, .0f, .0f, true);
    }
  }

  lua_pop(L, 2);

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
        lua_error(L);
        std::unreachable();
      }
    }
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_ref);
  lua_rawgeti(L, LUA_REGISTRYINDEX, handle);
  lua_setfield(L, -2, name.data());
  lua_pop(L, 1);
}

uint8_t scene::pick_at(float x, float y, entt::entity* buffer, uint8_t capacity) const noexcept {
  constexpr auto half = .5f;
  const b2AABB aabb = {{x - half, y - half}, {x + half, y + half}};

  struct context final {
    entt::entity* hits;
    uint8_t capacity;
    uint8_t count;
  };

  context ctx{buffer, capacity, 0};

  b2World_OverlapAABB(
    _world, aabb, filter,
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

entt::entity scene::find_topmost(std::span<const entt::entity> hits) const noexcept {
  if (hits.empty()) [[unlikely]]
    return entt::null;

  entt::entity topmost = entt::null;
  auto depth = std::numeric_limits<int>::min();
  auto rank = std::numeric_limits<std::size_t>::max();
  const auto* order = _registry.storage<renderable>();

  for (const auto entity : hits) {
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

void scene::dispatch_miss(int callback, float x, float y, const char* button) {
  if (callback == LUA_NOREF) [[likely]]
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
  lua_rawgeti(L, LUA_REGISTRYINDEX, _table_ref);
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

void scene::dispatch_press(float x, float y, const char* button) {
  std::array<entt::entity, picks> buffer{};
  const auto count = pick_at(x, y, buffer.data(), static_cast<uint8_t>(buffer.size()));

  if (count == 0) [[likely]] {
    dispatch_miss(_on_press_ref, x, y, button);
    return;
  }

  const auto topmost = find_topmost(std::span(buffer.data(), count));
  if (topmost == entt::null) [[unlikely]]
    return;

  const auto* proxy = _registry.try_get<scriptable>(topmost);
  if (!proxy || proxy->handle_ref == LUA_NOREF || proxy->blueprint->on_press_ref == LUA_NOREF) [[unlikely]]
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy->blueprint->on_press_ref);
  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy->handle_ref);
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

void scene::dispatch_release(float x, float y, const char* button) {
  std::array<entt::entity, picks> buffer{};
  const auto count = pick_at(x, y, buffer.data(), static_cast<uint8_t>(buffer.size()));

  if (count == 0) [[likely]] {
    dispatch_miss(_on_release_ref, x, y, button);
    return;
  }

  const auto topmost = find_topmost(std::span(buffer.data(), count));
  if (topmost == entt::null) [[unlikely]]
    return;

  const auto* proxy = _registry.try_get<scriptable>(topmost);
  if (!proxy || proxy->handle_ref == LUA_NOREF || proxy->blueprint->on_release_ref == LUA_NOREF) [[unlikely]]
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy->blueprint->on_release_ref);
  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy->handle_ref);
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

void scene::dispatch_hover(float x, float y) {
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
  if (!proxy || proxy->handle_ref == LUA_NOREF || proxy->blueprint->on_hover_ref == LUA_NOREF)
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy->blueprint->on_hover_ref);
  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy->handle_ref);
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

void scene::dispatch_unhover() {
  if (_hovered == entt::null) [[likely]]
    return;

  const auto entity = std::exchange(_hovered, entt::null);
  const auto* proxy = _registry.try_get<scriptable>(entity);
  if (!proxy || proxy->handle_ref == LUA_NOREF || proxy->blueprint->on_unhover_ref == LUA_NOREF)
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy->blueprint->on_unhover_ref);
  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy->handle_ref);
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
