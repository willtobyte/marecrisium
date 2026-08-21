static int navigate_callback(lua_State *state) {
  std::size_t length;
  const auto* data = luaL_checklstring(state, 1, &length);
  std::string name{data, length};
  auto *self = static_cast<director *>(lua_touserdata(state, lua_upvalueindex(1)));
  self->navigate(std::move(name));

  return 0;
}

static int destroy_callback(lua_State *state) {
  std::size_t length;
  const auto* data = luaL_checklstring(state, 1, &length);
  const std::string_view name{data, length};
  auto *self = static_cast<director *>(lua_touserdata(state, lua_upvalueindex(1)));
  self->destroy(name);

  return 0;
}

static int enroll_callback(lua_State *state) {
  std::size_t length;
  const auto* data = luaL_checklstring(state, 1, &length);
  std::string name{data, length};
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

void director::enroll(std::string name) {
  auto instance = std::make_unique<scene>(name);
  const auto [_, inserted] = _scenes.emplace(std::move(name), std::move(instance));
  assert(inserted && "scene must not already be enrolled");
  [[assume(inserted)]];
}

void director::destroy(std::string_view name) {
  const auto allowed = !_pending || *_pending != name;
  assert(allowed && "pending scene must not be destroyed");
  [[assume(allowed)]];

  auto it = _scenes.find(name);

  if (it == _scenes.end() || it->second.get() == _current) [[unlikely]]
    return;

  _scenes.erase(it);
}

void director::update(float delta) {
  if (_pending) [[unlikely]] {
    if (_current) [[unlikely]]
      _current->on_leave();

    const auto it = _scenes.find(*_pending);
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

  timer::update(delta);

  _current->update(delta);
}

void director::draw() {
  _current->draw();
}
