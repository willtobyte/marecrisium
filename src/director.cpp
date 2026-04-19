#include "director.hpp"

static int navigate_callback(lua_State *state) {
  std::string name = luaL_checkstring(state, 1);
  auto *self = static_cast<director *>(lua_touserdata(state, lua_upvalueindex(1)));
  self->navigate(std::move(name));
  return 0;
}

static int destroy_callback(lua_State *state) {
  const std::string_view name = luaL_checkstring(state, 1);
  auto *self = static_cast<director *>(lua_touserdata(state, lua_upvalueindex(1)));
  self->destroy(name);
  return 0;
}

static int enroll_callback(lua_State *state) {
  std::string name = luaL_checkstring(state, 1);
  auto *self = static_cast<director *>(lua_touserdata(state, lua_upvalueindex(1)));
  self->enroll(std::move(name));
  return 0;
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
  lua_pushcclosure(L, enroll_callback, 1);
  lua_setfield(L, -2, "enroll");

  lua_setglobal(L, "director");
}

void director::navigate(std::string name) {
  _pending = std::move(name);
}

void director::destroy(std::string_view name) {
  const auto key = entt::hashed_string{name.data()};
  auto it = _stages.find(key);

  if (it == _stages.end() || it->second.get() == _current) [[unlikely]]
    return;

  _stages.erase(it);
}

void director::set_overlay(std::string_view name) {
  const auto key = entt::hashed_string{name.data()};
  const auto [it, inserted] = _overlays.try_emplace(key, nullptr);

  if (inserted)
    it->second = std::make_unique<overlay>(name);

  _overlay = it->second.get();
  _overlay->expose();
}

void director::clear_overlay() {
  _overlay = nullptr;
}

void director::enroll(std::string name) {
  const auto key = entt::hashed_string{name.data()};
  const auto [it, inserted] = _stages.try_emplace(key);
  if (inserted)
    it->second = std::make_unique<stage>(std::move(name));
}

void director::transition() {
  if (!_pending) [[likely]] {
    return;
  }

  if (_current) [[likely]] {
    _current->on_leave();
    clear_overlay();
  }

  const auto key = entt::hashed_string{_pending->data()};
  auto [it, inserted] = _stages.try_emplace(key);
  if (inserted)
    it->second = std::make_unique<stage>(std::move(*_pending));

  _pending.reset();
  _current = it->second.get();
  _current->expose();

  if (const auto& o = _current->_overlay; o) {
    set_overlay(*o);

    if (const auto& f = _current->_foreground; f)
      _overlay->set_foreground(*f);
  }

  _current->on_enter();
}

void director::on_tick(uint64_t tick) {
  if (!_current) [[unlikely]]
    return;

  _current->on_tick(tick);
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
