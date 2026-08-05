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
  const auto key = entt::hashed_string{name.data(), name.size()};
  const auto allowed = !_pending || entt::hashed_string{_pending->data(), _pending->size()} != key;
  assert(allowed && "pending scene must not be destroyed");
  [[assume(allowed)]];

  auto it = _scenes.find(key);

  if (it == _scenes.end() || it->second.get() == _current) [[unlikely]]
    return;

  _scenes.erase(it);
}

void director::transition() {
  if (!_pending) [[likely]] {
    return;
  }

  if (_current) [[likely]]
    _current->on_leave();

  const auto key = entt::hashed_string{_pending->data(), _pending->size()};
  const auto it = _scenes.find(key);
  const auto found = it != _scenes.end();
  assert(found && "scene must be enrolled before navigation");
  [[assume(found)]];

  _pending.reset();
  _current = it->second.get();
  _current->_timer.activate();

  lua_rawgeti(L, LUA_REGISTRYINDEX, _current->_pool);
  lua_setglobal(L, "pool");

  _current->on_enter();
}

void director::update(float delta) {
  if (_current) [[likely]]
    _current->update(delta);
}

void director::draw() {
  if (_current) [[likely]]
    _current->draw();
}
