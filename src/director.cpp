#include "director.hpp"

static int navigate_callback(lua_State *state) {
  const auto *name = luaL_checkstring(state, 1);
  auto *self = static_cast<director *>(lua_touserdata(state, lua_upvalueindex(1)));
  self->navigate(name);
  return 0;
}

void director::wire() {
  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, navigate_callback, 1);
  lua_setglobal(L, "navigate");
}

void director::navigate(std::string_view name) {
  _pending = name;
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

void director::update(float delta) {
  if (_current) [[likely]] {
    _current->update(delta);
  }
}

void director::draw() const {
  if (_current) [[likely]] {
    _current->draw();
  }
}
