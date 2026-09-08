static int navigate_callback(lua_State *state) {
  std::size_t length;
  const auto* data = luaL_checklstring(state, 1, &length);
  const std::string_view name{data, length};
  auto *self = static_cast<director *>(lua_touserdata(state, lua_upvalueindex(1)));
  self->navigate(name);

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
  const std::string_view name{data, length};
  auto *self = static_cast<director *>(lua_touserdata(state, lua_upvalueindex(1)));
  self->enroll(name);

  return 0;
}

void director::wire() {
  lua_createtable(L, 0, 3);

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
  _pending = &_scenes.find(name)->second;
}

void director::enroll(std::string_view name) {
  _scenes.emplace(name, name);
}

void director::destroy(std::string_view name) {
  auto it = _scenes.find(name);

  if (it == _scenes.end() || &it->second == _current) [[unlikely]]
    return;

  _scenes.erase(it);
}

void director::update(float delta) {
  if (_pending) [[unlikely]] {
    if (_current) [[unlikely]]
      _current->on_leave();

    _current = _pending;
    _pending = nullptr;

    _current->on_enter();
  }

  _current->update(delta);
}

void director::draw() {
  _current->draw();
}
