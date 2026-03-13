#include "director.hpp"

static int navigate_callback(lua_State *state) {
  const auto *name = luaL_checkstring(state, 1);
  auto *self = static_cast<director *>(lua_touserdata(state, lua_upvalueindex(1)));
  self->navigate(name);
  return 0;
}

static int destroy_callback(lua_State *state) {
  const auto *name = luaL_checkstring(state, 1);
  auto *self = static_cast<director *>(lua_touserdata(state, lua_upvalueindex(1)));
  self->destroy(name);
  return 0;
}

static int preload_callback(lua_State *state) {
  const auto *name = luaL_checkstring(state, 1);
  auto *self = static_cast<director *>(lua_touserdata(state, lua_upvalueindex(1)));
  self->preload(name);
  return 0;
}

static int reset_callback(lua_State *state) {
  auto *self = static_cast<director *>(lua_touserdata(state, lua_upvalueindex(1)));
  self->reset();
  return 0;
}

static int newindex_callback(lua_State *state) {
  auto *self = static_cast<director *>(lua_touserdata(state, lua_upvalueindex(1)));
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

  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, navigate_callback, 1);
  lua_setfield(L, -2, "navigate");

  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, destroy_callback, 1);
  lua_setfield(L, -2, "destroy");

  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, preload_callback, 1);
  lua_setfield(L, -2, "preload");

  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, reset_callback, 1);
  lua_setfield(L, -2, "reset");

  luaL_newmetatable(L, "director");
  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, newindex_callback, 1);
  lua_setfield(L, -2, "__newindex");
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
  _overlay->wire();
}

void director::clear_overlay() {
  _overlay = nullptr;
}

void director::preload(std::string_view name) {
  if (_stages.find(name) != _stages.end()) [[unlikely]] {
    return;
  }

  _stages.try_emplace(std::string{name}, std::make_unique<stage>(name));
}

void director::transition() {
  if (!_pending) [[likely]] {
    return;
  }

  if (_current) [[likely]] {
    _current->on_leave();
  }

  auto it = _stages.find(*_pending);

  if (it == _stages.end()) {
    auto s = std::make_unique<stage>(*_pending);
    auto [inserted, _] = _stages.emplace(std::move(*_pending), std::move(s));
    it = inserted;
  }

  _pending.reset();
  _current = it->second.get();
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
