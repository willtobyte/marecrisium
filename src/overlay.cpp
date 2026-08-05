static int newindex(lua_State *state) {
  auto *self = *static_cast<overlay **>(luaL_checkudata(state, 1, "Foregrounds"));
  std::size_t length;
  const std::string_view name{luaL_checklstring(state, 2, &length), length};

  lua_toboolean(state, 3)
    ? self->show(name)
    : self->hide(name);

  return 0;
}

overlay::overlay() {
  auto **instance = static_cast<overlay **>(lua_newuserdata(L, sizeof(overlay *)));
  *instance = this;
  luaL_getmetatable(L, "Foregrounds");
  lua_setmetatable(L, -2);
  _userdata_ref = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _userdata_ref);
  lua_setglobal(L, "foregrounds");
}

overlay::~overlay() noexcept(false) {
  if (std::uncaught_exceptions() == 0)
    clear();
  luaL_unref(L, LUA_REGISTRYINDEX, _userdata_ref);
}

void overlay::wire() {
  luaL_newmetatable(L, "Foregrounds");
  lua_pushliteral(L, "Foregrounds");
  lua_setfield(L, -2, "__name");

  lua_pushcfunction(L, newindex);
  lua_setfield(L, -2, "__newindex");
  lua_pop(L, 1);
}

void overlay::show(std::string_view name) {
  const auto key = entt::hashed_string{name.data(), name.size()};
  auto it = _foregrounds.find(key);
  if (it == _foregrounds.end()) {
    auto fg = std::make_unique<foreground>(name);
    it = _foregrounds.try_emplace(key, std::move(fg)).first;
  }

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

  _active.erase(active);
  foreground->disappear();
}

void overlay::clear() {
  auto active = std::move(_active);
  _active = std::move(_snapshot);
  _active.clear();

  for (auto *fg : active) {
    if (std::ranges::find(_active, fg) == _active.end())
      fg->disappear();
  }

  _snapshot = std::move(active);
}

void overlay::update(float delta) {
  _snapshot.assign(_active.begin(), _active.end());
  for (auto *foreground : _snapshot)
    foreground->update(delta);
}

void overlay::draw() {
  _snapshot.assign(_active.begin(), _active.end());
  for (auto *foreground : _snapshot)
    foreground->draw();
}
