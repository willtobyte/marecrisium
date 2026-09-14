namespace {
constexpr auto stopped = -std::numeric_limits<float>::infinity();
}

scene::scene(std::string_view key)
    : _overlay(key),
      _background(std::format("blobs/scenes/{}/background.png", key)) {
  struct prior final {
    int pool;
    int timer;
  };

  lua_getglobal(L, "pool");
  const auto pool = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_getglobal(L, "timer");
  const auto timer = luaL_ref(L, LUA_REGISTRYINDEX);
  const prior prior{.pool = pool, .timer = timer};

  _scheduler.wire();
  lua_rawgeti(L, LUA_REGISTRYINDEX, _scheduler._table);
  lua_setglobal(L, "timer");

  SDL_SetTextureBlendMode(_background, SDL_BLENDMODE_NONE);

  const auto chunk = std::format("@scenes/{}.lua", key);
  const auto filename = std::string_view{chunk}.substr(1);
  const auto source = io::read(filename);

  if (luaL_loadbuffer(L, reinterpret_cast<const char*>(source.data()), source.size(), chunk.c_str()) != LUA_OK) [[unlikely]]
    throw std::runtime_error{lua_tostring(L, -1)};

  if (pcall(L, 0, 1) != LUA_OK) [[unlikely]]
    throw std::runtime_error{lua_tostring(L, -1)};

  int capacity{};

  lua_pushliteral(L, "objects");
  lua_rawget(L, -2);
  capacity += static_cast<int>(lua_objlen(L, -1));
  lua_pop(L, 1);

  lua_pushliteral(L, "sounds");
  lua_rawget(L, -2);
  capacity += static_cast<int>(lua_objlen(L, -1));
  lua_pop(L, 1);

  lua_createtable(L, 0, capacity);
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

      const auto* sheet = depot->get<spritesheet>(kind, L, -1);
      object.sprite.sheet = sheet;
      object.motion.active = sheet->initial;
      const auto& source = sheet->source;
      object.sprite.resize(source.width, source.height, object.sprite.scale);

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
        if (pcall(L, 1, 0) != LUA_OK) [[unlikely]]
          throw std::runtime_error{lua_tostring(L, -1)};
      }

      lua_pop(L, 3);
    }

    lua_pop(L, 1);
  }

  {
    lua_getfield(L, -1, "sounds");
    const auto length = static_cast<int>(lua_objlen(L, -1));

    for (auto i = 1; i <= length; ++i) {
      lua_rawgeti(L, -1, i);

      lua_getfield(L, -1, "name");
      std::size_t length;
      const auto* data = luaL_checklstring(L, -1, &length);
      const std::string_view name{data, length};

      auto& instance = _playback.add(name);
      auto **memory = static_cast<class sound **>(lua_newuserdata(L, sizeof(class sound *)));
      *memory = &instance;
      luaL_getmetatable(L, "Sound");
      lua_setmetatable(L, -2);

      lua_rawgeti(L, LUA_REGISTRYINDEX, _pool);
      lua_pushvalue(L, -3);
      lua_pushvalue(L, -3);
      lua_rawset(L, -3);
      lua_pop(L, 1);

      lua_pop(L, 1);

      lua_pop(L, 2);
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
    instance->object = nullptr;
    instance->dirty = nullptr;
    lua_pop(L, 1);

    luaL_unref(L, LUA_REGISTRYINDEX, object.script.label);
    luaL_unref(L, LUA_REGISTRYINDEX, object.script.instance);
    luaL_unref(L, LUA_REGISTRYINDEX, object.script.on_end);
  }

  luaL_unref(L, LUA_REGISTRYINDEX, _on_leave);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_enter);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_loop);
  luaL_unref(L, LUA_REGISTRYINDEX, _pool);
  luaL_unref(L, LUA_REGISTRYINDEX, _scheduler._table);
  luaL_unref(L, LUA_REGISTRYINDEX, _table);
}

void scene::on_enter() {
  _scheduler.activate();

  lua_rawgeti(L, LUA_REGISTRYINDEX, _pool);
  lua_setglobal(L, "pool");

  lua_rawgeti(L, LUA_REGISTRYINDEX, _scheduler._table);
  lua_setglobal(L, "timer");

  _overlay.appear();

  if (_on_enter != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_enter);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
    if (pcall(L, 1, 0) != LUA_OK) [[unlikely]]
      throw std::runtime_error{lua_tostring(L, -1)};
  }
}

void scene::update(float delta) {
  _playback.update();
  _scheduler.update(delta);

  if (_on_loop != LUA_NOREF) [[likely]] {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_loop);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
    lua_pushnumber(L, static_cast<lua_Number>(delta));
    if (pcall(L, 2, 0) != LUA_OK) [[unlikely]]
      throw std::runtime_error{lua_tostring(L, -1)};
  }

  for (auto it = _loops.rbegin(); it != _loops.rend(); ++it) {
    const auto& object = _objects[*it];
    const auto callback = object.script.blueprint->on_loop;
    lua_rawgeti(L, LUA_REGISTRYINDEX, callback);
    lua_rawgeti(L, LUA_REGISTRYINDEX, object.script.instance);
    lua_pushnumber(L, static_cast<lua_Number>(delta));
    if (pcall(L, 2, 0) != LUA_OK) [[unlikely]]
      throw std::runtime_error{lua_tostring(L, -1)};
  }

  for (auto& object : std::views::reverse(_objects)) {
    auto& motion = object.motion;
    const auto& sequence = object.sprite.sheet->sequences[motion.active];
    const auto& frame = object.sprite.sheet->frames[sequence.offset + motion.current];

    motion.elapsed += delta;
    if (motion.elapsed < frame.duration) [[likely]]
      continue;

    motion.elapsed -= frame.duration;
    if (++motion.current < sequence.count)
      continue;

    if (sequence.loop) {
      motion.current = 0;
    } else {
      motion.current = sequence.count - 1;
      motion.elapsed = stopped;
    }

    if (!motion.ending)
      continue;

    lua_rawgeti(L, LUA_REGISTRYINDEX, object.script.on_end);
    lua_rawgeti(L, LUA_REGISTRYINDEX, object.script.instance);
    lua_rawgeti(L, LUA_REGISTRYINDEX, sequence.name);
    if (pcall(L, 2, 0) != LUA_OK) [[unlikely]]
      throw std::runtime_error{lua_tostring(L, -1)};
  }

  _overlay.update(delta);

  if (_dirty.order) [[unlikely]] {
    std::sort(_order.begin(), _order.end(), [this](const auto left, const auto right) {
      const auto& lhs = _objects[left].sprite;
      const auto& rhs = _objects[right].sprite;

      return lhs.z != rhs.z ? lhs.z < rhs.z : left < right;
    });

    _dirty.order = false;
  }
}

void scene::draw() {
  _background.draw(.0f, .0f, viewport.width, viewport.height);

  for (const auto id : _order) {
    const auto& object = _objects[id];
    if (!object.sprite.shown) [[unlikely]]
      continue;

    const auto& sequence = object.sprite.sheet->sequences[object.motion.active];
    const auto& frame = object.sprite.sheet->frames[sequence.offset + object.motion.current];
    const auto* sheet = object.sprite.sheet->pixmap;
    const auto& sprite = object.sprite;
    const auto& source = sprite.sheet->source;
    const auto& bounds = sprite.bounds;
    const auto scale = sprite.scale;
    const auto mirror = std::to_underlying(sprite.mirror);
    const auto width = frame.width * scale;
    const auto height = frame.height * scale;

    const auto bx = std::floor(sprite.x) + bounds.x;
    const auto by = std::floor(sprite.y) + bounds.y;
    const auto ox = mirror & SDL_FLIP_HORIZONTAL
      ? source.width - frame.offset.x - frame.width
      : frame.offset.x;
    const auto oy = mirror & SDL_FLIP_VERTICAL
      ? source.height - frame.offset.y - frame.height
      : frame.offset.y;

    auto x = bx + ox * scale;
    auto y = by + oy * scale;

    if (const auto angle = sprite.angle; angle != .0f) [[unlikely]] {
      constexpr auto degree = std::numbers::pi_v<float> / 180.f;

      float sine, cosine;
      sincos(angle * degree, sine, cosine);

      const auto cx = bx + bounds.width * .5f;
      const auto cy = by + bounds.height * .5f;
      const auto dx = x + width * .5f - cx;
      const auto dy = y + height * .5f - cy;

      x = cx + dx * cosine - dy * sine - width * .5f;
      y = cy + dx * sine + dy * cosine - height * .5f;
    }

    sheet->draw(
      frame.u0 * sheet->width(),
      frame.v0 * sheet->height(),
      frame.width,
      frame.height,
      x,
      y,
      width,
      height,
      sprite.angle,
      sprite.alpha,
      sprite.mirror);
  }

#ifdef DEBUG
  SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);

  for (const auto id : _order) {
    const auto& object = _objects[id];
    if (!object.sprite.shown) [[unlikely]]
      continue;

    const auto& sequence = object.sprite.sheet->sequences[object.motion.active];
    const auto& frame = object.sprite.sheet->frames[sequence.offset + object.motion.current];
    const auto& bounds = object.sprite.bounds;
    const auto& collider = frame.collider;
    if (collider.width == .0f || collider.height == .0f)
      continue;

    const SDL_FRect rect = {
      std::floor(object.sprite.x) + bounds.x + (frame.offset.x + collider.offset.x) * object.sprite.scale,
      std::floor(object.sprite.y) + bounds.y + (frame.offset.y + collider.offset.y) * object.sprite.scale,
      collider.width * object.sprite.scale,
      collider.height * object.sprite.scale,
    };

    SDL_RenderRect(renderer, &rect);
  }

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
#endif

  _overlay.draw();
}

void scene::on_leave() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _scheduler._table);
  lua_setglobal(L, "timer");

  auto result = LUA_OK;
  if (_on_leave != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_leave);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _table);
    result = pcall(L, 1, 0);
  }

  if (result == LUA_OK)
    result = _overlay.disappear();

  _playback.stop();

  lua_pushnil(L);
  lua_setglobal(L, "pool");

  _scheduler.suspend();

  if (result != LUA_OK) [[unlikely]]
    throw std::runtime_error{lua_tostring(L, -1)};
}
