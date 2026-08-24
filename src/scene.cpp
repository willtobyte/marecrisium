scene::scene(std::string_view name)
    : _background(std::make_unique<pixmap>(std::format("blobs/scenes/{}/background.png", name))),
      _overlay(name) {
  struct prior final {
    int pool;
    int timer;
  };

  lua_getglobal(L, "pool");
  const auto pool = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_getglobal(L, "timer");
  const auto timer = luaL_ref(L, LUA_REGISTRYINDEX);
  const prior prior{.pool = pool, .timer = timer};

  callbacks::wire(_timer);
  lua_rawgeti(L, LUA_REGISTRYINDEX, _timer._table);
  lua_setglobal(L, "timer");

  SDL_SetTextureBlendMode(*_background, SDL_BLENDMODE_NONE);

  const auto chunk = std::format("@scenes/{}.lua", name);
  const auto path = std::string_view{chunk}.substr(1);
  const auto source = io::read(path);

  if (luaL_loadbuffer(L, reinterpret_cast<const char*>(source.data()), source.size(), chunk.c_str()) != LUA_OK) [[unlikely]]
    throw std::runtime_error{lua_tostring(L, -1)};

  if (lua_pcall(L, 0, 1, 0) != LUA_OK) [[unlikely]]
    throw std::runtime_error{lua_tostring(L, -1)};

  lua_newtable(L);
  _pool = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _pool);
  lua_setglobal(L, "pool");

  {
    lua_getfield(L, -1, "objects");
    const auto length = static_cast<int>(lua_objlen(L, -1));

    _objects.reserve(length);
    _order.reserve(length);
    _loops.reserve(length);

    for (auto i = 1; i <= length; ++i) {
      lua_rawgeti(L, -1, i);

      lua_getfield(L, -1, "x");
      const auto ox = static_cast<float>(luaL_optnumber(L, -1, .0));
      lua_pop(L, 1);

      lua_getfield(L, -1, "y");
      const auto oy = static_cast<float>(luaL_optnumber(L, -1, .0));
      lua_pop(L, 1);

      std::size_t length;
      const char* data;

      lua_getfield(L, -1, "name");
      data = luaL_checklstring(L, -1, &length);
      const std::string_view label{data, length};

      lua_getfield(L, -2, "kind");
      data = luaL_checklstring(L, -1, &length);
      const std::string_view kind{data, length};

      const auto id = static_cast<uint32_t>(_objects.size());
      _order.emplace_back(id);

      auto& object = _objects.emplace_back();
      object.sprite.z = i;
      object.sprite.x = ox;
      object.sprite.y = oy;

      objects::bind(object, _dirty, label, kind);
      if (object.script.blueprint->on_loop != LUA_NOREF)
        _loops.emplace_back(id);

      lua_rawgeti(L, LUA_REGISTRYINDEX, object.script.blueprint->table);
      lua_getfield(L, -1, "animation");
      assert(lua_istable(L, -1) && "object must define an animation table");

      const auto* sheet = depot->get<spritesheet>(kind, L, -1);
      object.sprite.sheet = sheet;
      object.motion.active = sheet->initial;

      lua_pop(L, 2);

      lua_rawgeti(L, LUA_REGISTRYINDEX, _pool);
      lua_pushvalue(L, -3);
      lua_rawgeti(L, LUA_REGISTRYINDEX, object.script.instance);
      lua_rawset(L, -3);
      lua_pop(L, 1);

      const auto& blueprint = *object.script.blueprint;
      if (blueprint.on_spawn != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, blueprint.on_spawn);
        lua_rawgeti(L, LUA_REGISTRYINDEX, object.script.instance);
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) [[unlikely]]
          throw std::runtime_error{lua_tostring(L, -1)};
      }

      lua_pop(L, 3);
    }

    lua_pop(L, 1);
  }

  {
    lua_getfield(L, -1, "sounds");
    const auto length = static_cast<int>(lua_objlen(L, -1));

    _sounds.reserve(length);

    for (auto i = 1; i <= length; ++i) {
      lua_rawgeti(L, -1, i);

      lua_getfield(L, -1, "name");
      std::size_t length;
      const auto* data = luaL_checklstring(L, -1, &length);
      const std::string_view label{data, length};

      const auto key = std::format("sounds/{}", label);
      const auto* asset = depot->get<pcm>(key);
      auto instance = std::make_unique<sound>(*asset);
      auto **memory = static_cast<class sound **>(lua_newuserdata(L, sizeof(class sound *)));
      *memory = instance.get();
      luaL_getmetatable(L, "Sound");
      lua_setmetatable(L, -2);

      lua_rawgeti(L, LUA_REGISTRYINDEX, _pool);
      lua_pushvalue(L, -3);
      lua_pushvalue(L, -3);
      lua_rawset(L, -3);
      lua_pop(L, 1);

      lua_pop(L, 1);

      lua_pop(L, 2);

      _sounds.emplace_back(std::move(instance));
    }

    lua_pop(L, 1);
  }

  _table = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _table);

  lua_getfield(L, -1, "on_enter");
  _on_enter = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_loop");
  _on_loop = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_getfield(L, -1, "on_leave");
  _on_leave = lua_isfunction(L, -1) ? luaL_ref(L, LUA_REGISTRYINDEX) : (lua_pop(L, 1), LUA_NOREF);

  lua_pop(L, 1);

  lua_rawgeti(L, LUA_REGISTRYINDEX, prior.pool);
  lua_setglobal(L, "pool");
  luaL_unref(L, LUA_REGISTRYINDEX, prior.pool);

  lua_rawgeti(L, LUA_REGISTRYINDEX, prior.timer);
  lua_setglobal(L, "timer");
  luaL_unref(L, LUA_REGISTRYINDEX, prior.timer);
}

scene::~scene() {
  for (auto& object : _objects) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, object.script.instance);
    auto* instance = static_cast<proxy*>(lua_touserdata(L, -1));
    const auto valid = instance != nullptr;
    assert(valid && "object handle must be an Object userdata");
    [[assume(valid)]];
    instance->object = nullptr;
    instance->dirty = nullptr;
    lua_pop(L, 1);

    luaL_unref(L, LUA_REGISTRYINDEX, object.script.label);
    luaL_unref(L, LUA_REGISTRYINDEX, object.script.instance);
  }

  luaL_unref(L, LUA_REGISTRYINDEX, _on_leave);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_enter);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_loop);
  luaL_unref(L, LUA_REGISTRYINDEX, _pool);
  luaL_unref(L, LUA_REGISTRYINDEX, _timer._table);
  luaL_unref(L, LUA_REGISTRYINDEX, _table);
}

void scene::on_enter() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _pool);
  lua_setglobal(L, "pool");

  lua_rawgeti(L, LUA_REGISTRYINDEX, _timer._table);
  lua_setglobal(L, "timer");

  _overlay.appear();

  if (_on_enter != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_enter);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) [[unlikely]]
      throw std::runtime_error{lua_tostring(L, -1)};
  }
}

void scene::update(float delta) {
  _timer.update(delta);

  if (_on_loop != LUA_NOREF) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_loop);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
    lua_pushnumber(L, static_cast<lua_Number>(delta));
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) [[unlikely]]
      throw std::runtime_error{lua_tostring(L, -1)};
  }

  for (auto it = _loops.rbegin(); it != _loops.rend(); ++it) {
    const auto& object = _objects[*it];
    const auto callback = object.script.blueprint->on_loop;
    lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
    lua_rawgeti(L, LUA_REGISTRYINDEX, object.script.instance);
    lua_pushnumber(L, static_cast<lua_Number>(delta));
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) [[unlikely]]
      throw std::runtime_error{lua_tostring(L, -1)};
  }

  for (auto it = _objects.rbegin(); it != _objects.rend(); ++it) {
    auto& object = *it;
    const auto& clip = object.sprite.sheet->clips[object.motion.active];
    const auto& frame = object.sprite.sheet->frames[clip.offset + object.motion.current];

    object.motion.elapsed += delta;
    if (object.motion.elapsed < frame.duration) [[likely]]
      continue;

    object.motion.elapsed -= frame.duration;
    if (++object.motion.current < clip.count)
      continue;

    object.motion.current = 0;
  }

  if (_dirty) [[unlikely]] {
    for (auto i = 1uz; i < _order.size(); ++i) {
      const auto value = _order[i];
      auto j = i;
      while (j && _objects[value].sprite.z < _objects[_order[j - 1]].sprite.z) {
        _order[j] = _order[j - 1];
        --j;
      }
      _order[j] = value;
    }

    _dirty = false;
  }

  _overlay.update(delta);
}

void scene::draw() {
  _background->draw(
    .0f, .0f,
    static_cast<float>(_background->width()), static_cast<float>(_background->height()),
    .0f, .0f,
    viewport.width, viewport.height
  );

  for (const auto id : _order) {
    const auto& object = _objects[id];
    if (!object.sprite.shown) [[unlikely]]
      continue;

    const auto& clip = object.sprite.sheet->clips[object.motion.active];
    const auto& frame = object.sprite.sheet->frames[clip.offset + object.motion.current];
    const auto* sheet = object.sprite.sheet->pixmap;

    sheet->draw(
      frame.u0 * static_cast<float>(sheet->width()),
      frame.v0 * static_cast<float>(sheet->height()),
      frame.width,
      frame.height,
      std::floor(object.sprite.x - viewport.x),
      std::floor(object.sprite.y - viewport.y),
      frame.width * object.sprite.scale,
      frame.height * object.sprite.scale,
      object.sprite.angle,
      static_cast<uint8_t>(std::clamp(object.sprite.alpha, .0f, 255.f)),
      object.sprite.mirror);
  }

  _overlay.draw();
}

void scene::on_leave() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _timer._table);
  lua_setglobal(L, "timer");

  auto result = LUA_OK;
  if (_on_leave != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_leave);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
    result = lua_pcall(L, 1, 0, 0);
  }

  lua_pushnil(L);
  lua_setglobal(L, "pool");

  for (const auto& sound : _sounds)
    sound->stop();

  if (result != LUA_OK) [[unlikely]]
    throw std::runtime_error{lua_tostring(L, -1)};

  _overlay.disappear();
}
