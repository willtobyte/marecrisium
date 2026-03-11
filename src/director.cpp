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

static int flush_callback(lua_State *state) {
  auto *self = static_cast<director *>(lua_touserdata(state, lua_upvalueindex(1)));
  self->flush();
  return 0;
}

static int overlay_callback(lua_State *state) {
  auto *self = static_cast<director *>(lua_touserdata(state, lua_upvalueindex(1)));

  if (lua_isnil(state, 1) || lua_isnone(state, 1)) {
    self->set_overlay(std::nullopt);
    return 0;
  }

  self->set_overlay(luaL_checkstring(state, 1));
  return 0;
}

director::~director() = default;

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
  lua_pushcclosure(L, flush_callback, 1);
  lua_setfield(L, -2, "flush");

  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, overlay_callback, 1);
  lua_setfield(L, -2, "overlay");

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

void director::flush() {
  _current = nullptr;

  _stages.clear();
  _overlays.clear();
  resources.source.clear();
  resources.sound.clear();
  resources.pixmap.clear();
  resources.font.clear();

  set_overlay(std::nullopt);
}

void director::set_overlay(std::optional<std::string_view> name) {
  if (!name) {
    _overlay = nullptr;
    return;
  }

  const auto [it, inserted] = _overlays.try_emplace(std::string{*name}, nullptr);

  if (inserted) {
    it->second = std::make_unique<overlay>(*name);
  }

  _overlay = it->second.get();
  _overlay->wire();
}

void director::preload(std::string_view name) {
  if (_stages.contains(name)) [[unlikely]] {
    return;
  }

  _stages.emplace(name, std::make_unique<stage>(name));
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

void director::draw() const {
  if (_current) [[likely]] {
    _current->draw();
  }

  if (_overlay) {
    _overlay->draw();
  }
}
