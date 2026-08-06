scene::scene(std::string name)
    : _name(std::move(name)),
      _background(std::make_unique<pixmap>(std::format("blobs/scenes/{}/background.png", _name))),
      _overlay(_name) {
  const timer::scope scope{_timer};

  SDL_SetTextureBlendMode(*_background, SDL_BLENDMODE_NONE);

  const auto chunk = std::format("@scenes/{}.lua", _name);
  const auto path = std::string_view{chunk}.substr(1);
  const auto source = io::read(path);

  if (luaL_loadbuffer(L, reinterpret_cast<const char*>(source.data()), source.size(), chunk.c_str()) != LUA_OK) [[unlikely]]
    lua_error(L);

  if (lua_pcall(L, 0, 1, 0) != LUA_OK) [[unlikely]]
    lua_error(L);

  lua_newtable(L);
  _pool = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_getglobal(L, "pool");
  const auto previous_pool = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_rawgeti(L, LUA_REGISTRYINDEX, _pool);
  lua_setglobal(L, "pool");

  lua_getfield(L, -1, "objects");
  const auto objects = static_cast<int>(lua_objlen(L, -1));
  const auto capacity = static_cast<std::size_t>(objects);
  _systems.prepare(capacity);

  for (auto i = 1; i <= objects; ++i) {
    lua_rawgeti(L, -1, i);

    lua_getfield(L, -1, "name");
    const std::string label{luaL_checkstring(L, -1)};
    lua_pop(L, 1);

    lua_getfield(L, -1, "kind");
    const std::string kind{luaL_checkstring(L, -1)};
    lua_pop(L, 1);

    lua_getfield(L, -1, "x");
    const auto ox = static_cast<float>(luaL_optnumber(L, -1, .0));
    lua_pop(L, 1);

    lua_getfield(L, -1, "y");
    const auto oy = static_cast<float>(luaL_optnumber(L, -1, .0));
    lua_pop(L, 1);

    lua_pop(L, 1);

    _systems.spawn(_pool, kind, label, ox, oy);
  }
  lua_pop(L, 1);

  lua_getfield(L, -1, "sounds");
  const auto sounds = static_cast<int>(lua_objlen(L, -1));

  for (auto i = 1; i <= sounds; ++i) {
    lua_rawgeti(L, -1, i);

    lua_getfield(L, -1, "name");
    const std::string label{luaL_checkstring(L, -1)};
    lua_pop(L, 1);

    lua_getfield(L, -1, "loop");
    const auto loop = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);

    const auto key = std::format("sounds/{}", label);
    auto *instance = depot->sound.get(key);
    auto **memory = static_cast<class sound **>(lua_newuserdata(L, sizeof(class sound *)));
    *memory = instance;
    luaL_getmetatable(L, "Sound");
    lua_setmetatable(L, -2);

    lua_rawgeti(L, LUA_REGISTRYINDEX, _pool);
    lua_pushvalue(L, -2);
    lua_setfield(L, -2, label.c_str());
    lua_pop(L, 1);

    lua_pop(L, 1);

    instance->set_loop(loop);

    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  _table = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _table);

  lua_getfield(L, -1, "on_loop");
  _on_loop = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_camera");
  _on_camera = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_enter");
  _on_enter = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_leave");
  _on_leave = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_press");
  _on_press = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_release");
  _on_release = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_pop(L, 1);

  lua_rawgeti(L, LUA_REGISTRYINDEX, previous_pool);
  lua_setglobal(L, "pool");
  luaL_unref(L, LUA_REGISTRYINDEX, previous_pool);
}

scene::~scene() {
  _registry.clear();

  luaL_unref(L, LUA_REGISTRYINDEX, _on_release);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_press);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_leave);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_enter);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_camera);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_loop);
  luaL_unref(L, LUA_REGISTRYINDEX, _pool);
  luaL_unref(L, LUA_REGISTRYINDEX, _table);
}

void scene::update(float delta) {
  float mx, my;
  const auto buttons = SDL_GetMouseState(&mx, &my);
  SDL_RenderCoordinatesFromWindow(renderer, mx, my, &mx, &my);
  mx += viewport.x;
  my += viewport.y;

  const auto object = _systems.pick(mx, my);
  _systems.hover(_hovered, object);

  _systems.press(_hovered, _table, _mouse_previous_buttons, buttons, mx, my, _on_press, _on_release);

  if (_on_loop != LUA_NOREF) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_loop);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
    lua_pushnumber(L, static_cast<lua_Number>(delta));
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) [[unlikely]]
      lua_error(L);
  }

  _systems.loop(delta);
  _systems.animate(delta);

  if (_on_camera != LUA_NOREF) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_camera);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);

    if (lua_pcall(L, 1, 2, 0) != LUA_OK) [[unlikely]]
      lua_error(L);

    if (lua_isnumber(L, -2))
      viewport.x = std::floor(static_cast<float>(lua_tonumber(L, -2)) * viewport.scale) / viewport.scale;
    if (lua_isnumber(L, -1))
      viewport.y = std::floor(static_cast<float>(lua_tonumber(L, -1)) * viewport.scale) / viewport.scale;
    lua_pop(L, 2);
  }

  depot->sound.poll();

  _systems.sort();

  _overlay.update(delta);
}

void scene::draw() {
  _background->draw(
    .0f, .0f,
    static_cast<float>(_background->width()), static_cast<float>(_background->height()),
    .0f, .0f,
    viewport.width, viewport.height
  );

  _systems.draw();

  _overlay.draw();
}

void scene::on_enter() {
  _overlay.appear();

  if (_on_enter != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_enter);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) [[unlikely]]
      lua_error(L);
  }
}

void scene::on_leave() {
  if (_on_leave != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_leave);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) [[unlikely]]
      lua_error(L);
  }

  _overlay.disappear();
}
