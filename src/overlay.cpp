static int overlay_newindex(lua_State *state) {
  auto *self = *static_cast<overlay **>(luaL_checkudata(state, 1, "Foregrounds"));
  std::size_t length;
  const std::string_view name{luaL_checklstring(state, 2, &length), length};

  if (lua_toboolean(state, 3) != 0) {
    self->show(name);
  } else {
    self->hide(name);
  }

  return 0;
}

overlay::overlay() {
  auto **instance = static_cast<overlay **>(lua_newuserdata(L, sizeof(overlay *)));
  *instance = this;
  luaL_getmetatable(L, "Foregrounds");
  lua_setmetatable(L, -2);
  _userdata_reference = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _userdata_reference);
  lua_setglobal(L, "foregrounds");
}

overlay::~overlay() {
  clear();
  luaL_unref(L, LUA_REGISTRYINDEX, _userdata_reference);
}

void overlay::wire() {
  binding::metatable(L, "Foregrounds", nullptr, overlay_newindex);
}

void overlay::show(std::string_view name) {
  const auto key = entt::hashed_string{name.data(), name.size()};
  const auto [it, inserted] = _foregrounds.try_emplace(key, nullptr);
  if (inserted)
    it->second = std::make_unique<foreground>(name);

  auto *fg = it->second.get();

  if (std::ranges::find(_active, fg) != _active.end()) [[unlikely]]
    return;

  _active.emplace_back(fg);
  fg->appear();
}

void overlay::hide(std::string_view name) {
  const auto key = entt::hashed_string{name.data(), name.size()};
  const auto it = _foregrounds.find(key);
  if (it == _foregrounds.end()) [[unlikely]]
    return;

  auto *foreground = it->second.get();
  const auto active = std::ranges::find(_active, foreground);
  if (active == _active.end())
    return;

  foreground->disappear();
  _active.erase(active);
}

void overlay::clear() {
  for (auto *foreground : std::exchange(_active, {}))
    foreground->disappear();
}

void overlay::update(float delta) {
  for (auto *foreground : decltype(_active){_active})
    foreground->update(delta);
}

void overlay::draw() {
  for (auto *foreground : decltype(_active){_active})
    foreground->draw();
}
