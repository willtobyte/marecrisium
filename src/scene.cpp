namespace {
template<typename T>
concept pushable = std::floating_point<T> || std::same_as<T, int>;

void push(float value) { lua_pushnumber(L, static_cast<lua_Number>(value)); }
void push(int reference) { lua_rawgeti(L, LUA_REGISTRYINDEX, reference); }

template<pushable... Args>
  requires (sizeof...(Args) > 0)
void invoke(int callback, Args... args) {
  const auto handler = lua_gettop(L) + 1;

  lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
  lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
  (push(args), ...);

  const auto status = lua_pcall(L, static_cast<int>(sizeof...(Args)), 0, handler);
  lua_remove(L, handler);

  if (status != LUA_OK) [[unlikely]] {
    lua_error(L);
    std::unreachable();
  }
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

static bool by_depth(const renderable& lhs, const renderable& rhs) {
  return lhs.z < rhs.z;
}

}

scene::scene(std::string name)
    : _name(std::move(name)) {
  const timer::scope scope{_timer};

  _registry.on_destroy<scriptable>().connect<&release_scriptable>();
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

      const auto entity = _registry.create();
      _registry.emplace<renderable>(entity, static_cast<int>(_registry.storage<renderable>().size()));

      auto& tf = _registry.emplace<transform>(entity);
      tf.x = ox;
      tf.y = oy;

      auto& op = _registry.emplace<scriptable>(entity);
      object::bind(_registry, entity, op, label, kind);
      const auto prototype = op.blueprint->table_ref;
      const auto handle = op.handle_ref;
      const auto on_spawn = op.blueprint->on_spawn_ref;

      lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);
      lua_getfield(L, -1, "animation");
      assert(lua_istable(L, -1) && "object must define an animation table");

      const auto* sheet = depot->spritesheet.get(kind, L, -1);
      auto& a = _registry.emplace<animation>(entity);
      a.sheet = sheet;
      a.active = sheet->initial;

      lua_pop(L, 2);

      if (on_spawn != LUA_NOREF) [[unlikely]]
        invoke(on_spawn, handle);

      lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_ref);
      lua_rawgeti(L, LUA_REGISTRYINDEX, handle);
      lua_setfield(L, -2, label.c_str());
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

  if (luaL_newmetatable(L, "Scene")) {
    lua_pushliteral(L, "Scene");
    lua_setfield(L, -2, "__name");
  }
  lua_setmetatable(L, -2);

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

  _registry.clear();
}

void scene::update(float delta) {
  float mx, my;
  const auto buttons = SDL_GetMouseState(&mx, &my);
  SDL_RenderCoordinatesFromWindow(renderer, mx, my, &mx, &my);
  mx += viewport.x;
  my += viewport.y;

  const auto object = pick(mx, my);
  const auto* over = object == entt::null ? nullptr : &_registry.get<scriptable>(object);

  if (object != _hovered) [[unlikely]] {
    const auto* left = _hovered == entt::null ? nullptr : &_registry.get<scriptable>(_hovered);
    _hovered = object;

    if (left && left->blueprint->on_unhover_ref != LUA_NOREF)
      invoke(left->blueprint->on_unhover_ref, left->handle_ref);

    if (over && over->blueprint->on_hover_ref != LUA_NOREF)
      invoke(over->blueprint->on_hover_ref, over->handle_ref);
  }

  const auto toggled = (buttons ^ _mouse_previous_buttons)
    & (SDL_BUTTON_LMASK | SDL_BUTTON_MMASK | SDL_BUTTON_RMASK);
  _mouse_previous_buttons = buttons;

  const auto self = over ? over->handle_ref : _table_ref;
  const auto press = over ? over->blueprint->on_press_ref : _on_press_ref;
  const auto release = over ? over->blueprint->on_release_ref : _on_release_ref;

  for (auto bits = toggled; bits; bits &= bits - 1) {
    const auto index = static_cast<size_t>(std::countr_zero(bits));
    const auto slot = (buttons >> index) & 1u ? press : release;

    if (slot != LUA_NOREF)
      invoke(slot, self, mx, my, mouse::labels[index]);
  }

  if (_on_loop_ref != LUA_NOREF) [[likely]]
    invoke(_on_loop_ref, _table_ref, delta);

  for (auto&& [e, op] : _registry.view<scriptable>().each()) {
    const auto& bp = *op.blueprint;

    if (bp.on_loop_ref != LUA_NOREF)
      invoke(bp.on_loop_ref, op.handle_ref, delta);

    auto* a = _registry.try_get<animation>(e);
    if (!a) [[unlikely]]
      continue;

    const auto& clip = a->sheet->clips[a->active];
    const auto& frame = a->sheet->frames[clip.offset + a->current];

    a->elapsed += delta;
    if (a->elapsed < frame.duration) [[likely]]
      continue;

    a->elapsed -= frame.duration;
    if (++a->current < clip.count)
      continue;

    a->current = 0;

    if (bp.on_animation_end_ref != LUA_NOREF)
      invoke(bp.on_animation_end_ref, op.handle_ref, clip.identity.name_ref);

    if (bp.on_animation_begin_ref != LUA_NOREF)
      invoke(bp.on_animation_begin_ref, op.handle_ref, clip.identity.name_ref);
  }

  if (_on_camera_ref != LUA_NOREF) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_camera_ref);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table_ref);

    const auto base = lua_gettop(L) - 1;
    lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
    lua_insert(L, base);
    const auto status = lua_pcall(L, 1, 2, base);
    lua_remove(L, base);

    if (status != LUA_OK) [[unlikely]] {
      lua_error(L);
      std::unreachable();
    }

    if (lua_isnumber(L, -2))
      viewport.x = std::floor(static_cast<float>(lua_tonumber(L, -2)) * viewport.scale) / viewport.scale;
    if (lua_isnumber(L, -1))
      viewport.y = std::floor(static_cast<float>(lua_tonumber(L, -1)) * viewport.scale) / viewport.scale;
    lua_pop(L, 2);
  }

  for (auto* sound : _sounds)
    sound->poll();

  auto& order = _registry.ctx().get<reorder>();
  if (order.dirty) [[unlikely]] {
    _registry.sort<renderable>(by_depth, entt::insertion_sort{});
    order.dirty = false;
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
}

void scene::on_enter() {
  if (_on_enter_ref != LUA_NOREF)
    invoke(_on_enter_ref, _table_ref);
}

void scene::on_leave() {
  if (_on_leave_ref != LUA_NOREF)
    invoke(_on_leave_ref, _table_ref);
}

entt::entity scene::pick(float x, float y) noexcept {
  const entt::sparse_set& order = _registry.storage<renderable>();

  for (auto it = order.rbegin(); it != order.rend(); ++it) {
    const auto entity = *it;
    const auto& tf = _registry.get<transform>(entity);
    if (!tf.shown || tf.alpha <= .0f) [[unlikely]]
      continue;

    const auto& a = _registry.get<animation>(entity);
    const auto b = bounds_of(a.sheet->frames[a.sheet->clips[a.active].offset + a.current], tf);
    if (x < b.x || x >= b.x + b.width) [[likely]]
      continue;
    if (y < b.y || y >= b.y + b.height) [[likely]]
      continue;

    return entity;
  }

  return entt::null;
}
