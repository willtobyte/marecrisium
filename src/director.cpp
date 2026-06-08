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


void director::navigate(std::string_view name) {
  _pending = name;
}

void director::destroy(std::string_view name) {
  const auto key = entt::hashed_string{name.data(), name.size()};
  auto it = _stages.find(key);

  if (it == _stages.end() || it->second.get() == _current) [[unlikely]]
    return;

  _stages.erase(it);
}

void director::transition() {
  if (!_pending) [[likely]] {
    return;
  }

  if (_current) [[likely]] {
    _current->on_leave();
    _overlay.clear();
  }

  const auto key = entt::hashed_string{_pending->data(), _pending->size()};
  auto [it, inserted] = _stages.try_emplace(key);
  if (inserted)
    it->second = std::make_unique<stage>(std::move(*_pending));

  _pending.reset();
  _current = it->second.get();
  _current->expose();

  for (const auto &name : _current->_foregrounds)
    _overlay.show(name);

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

  _overlay.update(delta);
}

void director::draw() {
  if (_current) [[likely]] {
    _current->draw();
  }

  _overlay.draw();
}
