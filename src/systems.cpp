namespace {

static void release(entt::registry& registry, entt::entity entity) {
  auto& op = registry.get<scriptable>(entity);

  lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle);
  auto* handle = static_cast<proxy*>(luaL_testudata(L, -1, "Object"));
  if (handle)
    handle->registry = nullptr;
  lua_pop(L, 1);

  luaL_unref(L, LUA_REGISTRYINDEX, op.label);
  luaL_unref(L, LUA_REGISTRYINDEX, op.handle);
}

constexpr bool by_depth(const renderable& lhs, const renderable& rhs) noexcept {
  return lhs.z < rhs.z;
}

}

void systems::prepare(std::size_t capacity) {
  _registry.on_destroy<scriptable>().connect<&release>();
  _registry.ctx().emplace<reorder>();

  _registry.storage<animation>();
  _registry.storage<renderable>();
  _registry.storage<scriptable>();
  _registry.storage<transform>();
  _registry.storage<entt::entity>().reserve(capacity);
  for (auto&& [_, storage] : _registry.storage())
    storage.reserve(capacity);
}

entt::entity systems::spawn(int pool, std::string_view kind, const std::string& label, float x, float y) {
  const auto entity = _registry.create();
  _registry.emplace<renderable>(entity, static_cast<int>(_registry.storage<renderable>().size()));

  auto& tf = _registry.emplace<transform>(entity);
  tf.x = x;
  tf.y = y;

  auto& op = _registry.emplace<scriptable>(entity);
  object::bind(_registry, entity, op, label, kind);

  lua_rawgeti(L, LUA_REGISTRYINDEX, op.blueprint->table);
  lua_getfield(L, -1, "animation");
  assert(lua_istable(L, -1) && "object must define an animation table");

  const auto* sheet = depot->spritesheet.get(kind, L, -1);
  auto& a = _registry.emplace<animation>(entity);
  a.sheet = sheet;
  a.active = sheet->initial;

  lua_pop(L, 2);

  lua_rawgeti(L, LUA_REGISTRYINDEX, pool);
  lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle);
  lua_setfield(L, -2, label.c_str());
  lua_pop(L, 1);

  return entity;
}

entt::entity systems::pick(float x, float y) noexcept {
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

void systems::hover(entt::entity& hovered, entt::entity picked) {
  if (picked == hovered)
    return;

  const auto* left = hovered == entt::null ? nullptr : &_registry.get<scriptable>(hovered);
  const auto* over = picked == entt::null ? nullptr : &_registry.get<scriptable>(picked);
  hovered = picked;

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

void systems::loop(float delta) {
  for (auto&& [e, op] : _registry.view<scriptable>().each()) {
    const auto& bp = *op.blueprint;

    if (bp.on_loop != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, bp.on_loop);
      lua_rawgeti(L, LUA_REGISTRYINDEX, op.handle);
      lua_pushnumber(L, static_cast<lua_Number>(delta));
      if (lua_pcall(L, 2, 0, 0) != LUA_OK) [[unlikely]]
        lua_error(L);
    }
  }
}

void systems::animate(float delta) {
  for (auto&& [e, op] : _registry.view<scriptable>().each()) {
    auto* a = _registry.try_get<animation>(e);
    if (!a) [[unlikely]]
      continue;

    const auto& bp = *op.blueprint;
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
}

void systems::sort() {
  auto& order = _registry.ctx().get<::reorder>();

  if (order.dirty) [[unlikely]] {
    _registry.sort<renderable>(by_depth, entt::insertion_sort{});
    order.dirty = false;
  }
}

void systems::draw() const {
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

void systems::press(entt::entity hovered, int table, uint32_t& previous, uint32_t buttons, float mx, float my, int on_press, int on_release) {
  const auto toggled = (buttons ^ previous) & (SDL_BUTTON_LMASK | SDL_BUTTON_MMASK | SDL_BUTTON_RMASK);
  previous = buttons;

  const auto* over = hovered == entt::null ? nullptr : &_registry.get<scriptable>(hovered);
  const auto self = over ? over->handle : table;
  const auto press = over ? over->blueprint->on_press : on_press;
  const auto release = over ? over->blueprint->on_release : on_release;

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
}
