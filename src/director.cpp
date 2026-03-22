#include "director.hpp"

static int navigate_callback(lua_State *state) {
  const auto *name = luaL_checkstring(state, 1);
  auto *self = upvalue<director>(state);
  self->navigate(name);
  return 0;
}

static int destroy_callback(lua_State *state) {
  const auto *name = luaL_checkstring(state, 1);
  auto *self = upvalue<director>(state);
  self->destroy(name);
  return 0;
}

static int preload_callback(lua_State *state) {
  const auto *name = luaL_checkstring(state, 1);
  auto *self = upvalue<director>(state);
  self->preload(name);
  return 0;
}

static int reset_callback(lua_State *state) {
  auto *self = upvalue<director>(state);
  self->reset();
  return 0;
}

static int newindex_callback(lua_State *state) {
  auto *self = upvalue<director>(state);
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "overlay") {
    if (lua_isnil(state, 3) || lua_isnone(state, 3)) {
      self->clear_overlay();
    } else {
      self->set_overlay(luaL_checkstring(state, 3));
    }

    return 0;
  }

  return luaL_error(state, "director: unknown property '%s'", key.data());
}

void director::wire() {
  lua_newtable(L);

  bind(L, "navigate", navigate_callback, this);
  bind(L, "destroy", destroy_callback, this);
  bind(L, "preload", preload_callback, this);
  bind(L, "reset", reset_callback, this);

  luaL_newmetatable(L, "director");
  bind(L, "__newindex", newindex_callback, this);
  lua_setmetatable(L, -2);

  lua_setglobal(L, "director");
}

void director::navigate(std::string_view name) {
  _pending = name;
}

void director::destroy(std::string_view name) {
  auto it = _stages.find(name);

  if (it == _stages.end() || it->second.get() == _current) [[unlikely]] {
    return;
  }

  _stages.erase(it);
}

void director::reset() {
  _stages.clear();
  _overlays.clear();
  depot->source.clear();
  depot->sound.clear();
  depot->pixmap.clear();
  depot->particle.clear();
  depot->font.clear();

  _current = nullptr;
  _overlay = nullptr;
}

void director::set_overlay(std::string_view name) {
  const auto [it, inserted] = _overlays.try_emplace(std::string{name}, nullptr);

  if (inserted) {
    it->second = std::make_unique<overlay>(name);
  }

  _overlay = it->second.get();
  _overlay->expose();
}

void director::clear_overlay() {
  _overlay = nullptr;
}

void director::preload(std::string_view name) {
  _stages.try_emplace(std::string{name}, std::make_unique<stage>(name));
}

void director::transition() {
  if (!_pending) [[likely]] {
    return;
  }

  if (_current) [[likely]] {
    _current->on_leave();
    clear_overlay();
  }

  auto [it, inserted] = _stages.try_emplace(*_pending);
  if (inserted)
    it->second = std::make_unique<stage>(it->first);

  _pending.reset();
  _current = it->second.get();

  if (const auto& o = _current->overlay(); o.has_value()) {
    set_overlay(o.value());

    if (const auto& f = _current->foreground(); f.has_value())
      _overlay->set_foreground(f.value());
  }

  _current->on_enter();
}

void director::on_tick(uint64_t tick) {
  if (_current) [[likely]] {
    _current->on_tick(tick);
  }
}

void director::update(float delta) {
  if (_current) [[likely]] {
    _current->update(delta);
  }

  if (_overlay) {
    _overlay->update(delta);
  }
}

void director::draw() {
  if (_current) [[likely]] {
    _current->draw();
  }

  if (_overlay) {
    _overlay->draw();
  }
}
