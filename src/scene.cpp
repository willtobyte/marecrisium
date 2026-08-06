namespace {
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

static bool by_depth(const renderable& lhs, const renderable& rhs) {
  return lhs.z < rhs.z;
}

}

scene::scene(std::string name)
    : _name(std::move(name)),
      _background(std::make_unique<pixmap>(std::format("blobs/scenes/{}/background.png", _name))),
      _overlay(_name) {
  const timer::scope scope{_timer};

  SDL_SetTextureBlendMode(*_background, SDL_BLENDMODE_NONE);

  _registry.on_destroy<scriptable>().connect<&release_scriptable>();
  _registry.ctx().emplace<reorder>();

  const auto chunk = std::format("@scenes/{}.lua", _name);
  const auto path = std::string_view{chunk}.substr(1);
  const auto source = io::read(path);

  if (luaL_loadbuffer(L, reinterpret_cast<const char*>(source.data()), source.size(), chunk.c_str()) != LUA_OK) [[unlikely]]
    lua_error(L);

  if (lua_pcall(L, 0, 1, 0) != LUA_OK) [[unlikely]]
    lua_error(L);

  lua_newtable(L);
  _pool = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_getglobal(L, "pool");
  const auto previous_pool = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_rawgeti(L, LUA_REGISTRYINDEX, _pool);
  lua_setglobal(L, "pool");

  lua_getfield(L, -1, "objects");
  const auto objects = static_cast<int>(lua_objlen(L, -1));
  const auto capacity = static_cast<std::size_t>(objects);
  _registry.storage<animation>();
  _registry.storage<renderable>();
  _registry.storage<scriptable>();
  _registry.storage<transform>();
  _registry.storage<entt::entity>().reserve(capacity);
  for (auto&& [_, storage] : _registry.storage())
    storage.reserve(capacity);

  for (auto i = 1; i <= objects; ++i) {
    lua_rawgeti(L, -1, i);

    lua_getfield(L, -1, "name");
    const std::string label{luaL_checkstring(L, -1)};
    lua_pop(L, 1);

    lua_getfield(L, -1, "kind");
    const std::string kind{luaL_checkstring(L, -1)};
    lua_pop(L, 1);

    lua_getfield(L, -1, "x");
    const auto ox = static_cast<float>(luaL_optnumber(L, -1, .0));
    lua_pop(L, 1);

    lua_getfield(L, -1, "y");
    const auto oy = static_cast<float>(luaL_optnumber(L, -1, .0));
    lua_pop(L, 1);

    lua_pop(L, 1);

    const auto entity = _registry.create();
    _registry.emplace<renderable>(entity, static_cast<int>(_registry.storage<renderable>().size()));

    auto& tf = _registry.emplace<transform>(entity);
    tf.x = ox;
    tf.y = oy;

    auto& op = _registry.emplace<scriptable>(entity);
    object::bind(_registry, entity, op, label, kind);
    const auto prototype = op.blueprint->table;
    const auto handle = op.handle;
    const auto on_spawn = op.blueprint->on_spawn;

    lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);
    lua_getfield(L, -1, "animation");
    assert(lua_istable(L, -1) && "object must define an animation table");

    const auto* sheet = depot->spritesheet.get(kind, L, -1);
    auto& a = _registry.emplace<animation>(entity);
    a.sheet = sheet;
    a.active = sheet->initial;

    lua_pop(L, 2);

    lua_rawgeti(L, LUA_REGISTRYINDEX, _pool);
    lua_rawgeti(L, LUA_REGISTRYINDEX, handle);
    lua_setfield(L, -2, label.c_str());
    lua_pop(L, 1);

    if (on_spawn != LUA_NOREF) [[unlikely]] {
      lua_rawgeti(L, LUA_REGISTRYINDEX, on_spawn);
      lua_rawgeti(L, LUA_REGISTRYINDEX, handle);
      if (lua_pcall(L, 1, 0, 0) != LUA_OK) [[unlikely]]
        lua_error(L);
    }
  }
  lua_pop(L, 1);

  lua_getfield(L, -1, "sounds");
  const auto sounds = static_cast<int>(lua_objlen(L, -1));

  for (auto i = 1; i <= sounds; ++i) {
    lua_rawgeti(L, -1, i);

    lua_getfield(L, -1, "name");
    const std::string label{luaL_checkstring(L, -1)};
    lua_pop(L, 1);

    lua_getfield(L, -1, "loop");
    const auto loop = lua_toboolean(L, -1) != 0;
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

    lua_pop(L, 1);

    instance->set_loop(loop);

    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  _table = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _table);

  lua_getfield(L, -1, "on_loop");
  _on_loop = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_camera");
  _on_camera = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_enter");
  _on_enter = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_leave");
  _on_leave = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_press");
  _on_press = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_release");
  _on_release = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_pop(L, 1);

  lua_rawgeti(L, LUA_REGISTRYINDEX, previous_pool);
  lua_setglobal(L, "pool");
  luaL_unref(L, LUA_REGISTRYINDEX, previous_pool);
}

scene::~scene() {
  _registry.clear();

  luaL_unref(L, LUA_REGISTRYINDEX, _on_release);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_press);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_leave);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_enter);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_camera);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_loop);
  luaL_unref(L, LUA_REGISTRYINDEX, _pool);
  luaL_unref(L, LUA_REGISTRYINDEX, _table);
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

    if (left && left->blueprint->on_unhover != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, left->blueprint->on_unhover);
      lua_rawgeti(L, LUA_REGISTRYINDEX, left->handle);
      if (lua_pcall(L, 1, 0, 0) != LUA_OK) [[unlikely]]
        lua_error(L);
    }

    if (over && over->blueprint->on_hover != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, over->blueprint->on_hover);
      lua_rawgeti(L, LUA_REGISTRYINDEX, over->handle);
      if (lua_pcall(L, 1, 0, 0) != LUA_OK) [[unlikely]]
        lua_error(L);
    }
  }

  const auto toggled = (buttons ^ _mouse_previous_buttons)
    & (SDL_BUTTON_LMASK | SDL_BUTTON_MMASK | SDL_BUTTON_RMASK);
  _mouse_previous_buttons = buttons;

  const auto self = over ? over->handle : _table;
  const auto press = over ? over->blueprint->on_press : _on_press;
  const auto release = over ? over->blueprint->on_release : _on_release;

  for (auto bits = toggled; bits; bits &= bits - 1) {
    const auto index = static_cast<size_t>(std::countr_zero(bits));
    const auto slot = (buttons >> index) & 1u ? press : release;

    if (slot != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, slot);
      lua_rawgeti(L, LUA_REGISTRYINDEX, self);
      lua_pushnumber(L, static_cast<lua_Number>(mx));
      lua_pushnumber(L, static_cast<lua_Number>(my));
      lua_rawgeti(L, LUA_REGISTRYINDEX, mouse::labels[index]);
      if (lua_pcall(L, 4, 0, 0) != LUA_OK) [[unlikely]]
        lua_error(L);
    }
  }

  if (_on_loop != LUA_NOREF) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_loop);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
    lua_pushnumber(L, static_cast<lua_Number>(delta));
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) [[unlikely]]
      lua_error(L);
  }

  for (auto&& [e, op] : _registry.view<scriptable>().each()) {
    const auto& bp = *op.blueprint;

    if (bp.on_loop != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, bp.on_loop);
      lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle);
      lua_pushnumber(L, static_cast<lua_Number>(delta));
      if (lua_pcall(L, 2, 0, 0) != LUA_OK) [[unlikely]]
        lua_error(L);
    }

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

    if (bp.on_animation_end != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, bp.on_animation_end);
      lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle);
      lua_rawgeti(L, LUA_REGISTRYINDEX, clip.identity.name);
      if (lua_pcall(L, 2, 0, 0) != LUA_OK) [[unlikely]]
        lua_error(L);
    }

    if (bp.on_animation_begin != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, bp.on_animation_begin);
      lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle);
      lua_rawgeti(L, LUA_REGISTRYINDEX, clip.identity.name);
      if (lua_pcall(L, 2, 0, 0) != LUA_OK) [[unlikely]]
        lua_error(L);
    }
  }

  if (_on_camera != LUA_NOREF) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_camera);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);

    if (lua_pcall(L, 1, 2, 0) != LUA_OK) [[unlikely]]
      lua_error(L);

    if (lua_isnumber(L, -2))
      viewport.x = std::floor(static_cast<float>(lua_tonumber(L, -2)) * viewport.scale) / viewport.scale;
    if (lua_isnumber(L, -1))
      viewport.y = std::floor(static_cast<float>(lua_tonumber(L, -1)) * viewport.scale) / viewport.scale;
    lua_pop(L, 2);
  }

  depot->sound.poll();

  auto& order = _registry.ctx().get<reorder>();
  if (order.dirty) [[unlikely]] {
    _registry.sort<renderable>(by_depth, entt::insertion_sort{});
    order.dirty = false;
  }

  _overlay.update(delta);
}

void scene::draw() {
  _background->draw(
    .0f, .0f,
    static_cast<float>(_background->width()), static_cast<float>(_background->height()),
    .0f, .0f,
    viewport.width, viewport.height
  );

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

  _overlay.draw();
}

void scene::on_enter() {
  _overlay.appear();

  if (_on_enter != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_enter);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) [[unlikely]]
      lua_error(L);
  }
}

void scene::on_leave() {
  if (_on_leave != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_leave);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) [[unlikely]]
      lua_error(L);
  }

  _overlay.disappear();
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
