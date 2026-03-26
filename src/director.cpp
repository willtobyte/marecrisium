#include "director.hpp"

static int navigate_callback(lua_State *state) {
  auto name = std::string{luaL_checkstring(state, 1)};
  auto *self = static_cast<director *>(lua_touserdata(state, lua_upvalueindex(1)));
  self->navigate(std::move(name));
  return 0;
}

static int destroy_callback(lua_State *state) {
  const auto name = std::string_view{luaL_checkstring(state, 1)};
  auto *self = static_cast<director *>(lua_touserdata(state, lua_upvalueindex(1)));
  self->destroy(name);
  return 0;
}

static int enroll_callback(lua_State *state) {
  auto name = std::string{luaL_checkstring(state, 1)};
  auto *self = static_cast<director *>(lua_touserdata(state, lua_upvalueindex(1)));
  self->enroll(std::move(name));
  return 0;
}

static int newindex_callback(lua_State *state) {
  auto *self = static_cast<director *>(lua_touserdata(state, lua_upvalueindex(1)));
  const auto key = std::string_view{luaL_checkstring(state, 2)};

  if (key == "overlay") {
    if (lua_isnil(state, 3) || lua_isnone(state, 3)) {
      self->clear_overlay();
    } else {
      self->set_overlay(std::string{luaL_checkstring(state, 3)});
    }

    return 0;
  }

  return luaL_error(state, "director: unknown property '%s'", key.data());
}

static void bind_closure(lua_State *state, const char *name, lua_CFunction fn, void *ptr) noexcept {
  lua_pushlightuserdata(state, ptr);
  lua_pushcclosure(state, fn, 1);
  lua_setfield(state, -2, name);
}

void director::wire() {
  lua_newtable(L);

  bind_closure(L, "navigate", navigate_callback, this);
  bind_closure(L, "destroy", destroy_callback, this);
  bind_closure(L, "enroll", enroll_callback, this);

  luaL_newmetatable(L, "director");
  bind_closure(L, "__newindex", newindex_callback, this);
  lua_setmetatable(L, -2);

  lua_setglobal(L, "director");
}

void director::navigate(std::string name) {
  _pending = std::move(name);
}

void director::destroy(std::string_view name) {
  auto it = _stages.find(name);

  if (it == _stages.end() || it->second.get() == _current) [[unlikely]] {
    return;
  }

  _stages.erase(it);
}

void director::set_overlay(std::string name) {
  const auto [it, inserted] = _overlays.try_emplace(std::move(name), nullptr);

  if (inserted) {
    it->second = std::make_unique<overlay>(it->first);
  }

  _overlay = it->second.get();
  _overlay->expose();
}

void director::clear_overlay() {
  _overlay = nullptr;
}

void director::enroll(std::string name) {
  const auto [it, inserted] = _stages.try_emplace(std::move(name));
  if (inserted)
    it->second = std::make_unique<stage>(it->first);
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
  _current->expose();

  if (const auto& o = _current->overlay(); o.has_value()) {
    set_overlay(o.value());

    if (const auto& f = _current->foreground(); f.has_value())
      _overlay->set_foreground(f.value());
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
