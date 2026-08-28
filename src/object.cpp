namespace {
std::unordered_map<std::string, std::unique_ptr<prototype>, transparent_string_hash, std::equal_to<>> prototypes;

static int index(lua_State* state) {
  const auto* self = static_cast<proxy*>(lua_touserdata(state, 1));
  std::size_t length;
  const auto* data = lua_tolstring(state, 2, &length);
  const std::string_view key{data, length};

  const auto alive = self->object != nullptr;
  assert(alive && "object must be alive when a property is read");
  [[assume(alive)]];

  const auto& object = *self->object;

  if (key == "x") {
    lua_pushnumber(state, static_cast<lua_Number>(object.sprite.x));
    return 1;
  }

  if (key == "y") {
    lua_pushnumber(state, static_cast<lua_Number>(object.sprite.y));
    return 1;
  }

  if (key == "z") {
    lua_pushinteger(state, static_cast<lua_Integer>(object.sprite.z));
    return 1;
  }

  if (key == "mirror") {
    lua_pushinteger(state, static_cast<lua_Integer>(object.sprite.mirror));
    return 1;
  }

  if (key == "shown") {
    lua_pushboolean(state, object.sprite.shown);
    return 1;
  }

  if (key == "scale") {
    lua_pushnumber(state, static_cast<lua_Number>(object.sprite.scale));
    return 1;
  }

  if (key == "angle") {
    lua_pushnumber(state, static_cast<lua_Number>(object.sprite.angle));
    return 1;
  }

  if (key == "alpha") {
    lua_pushnumber(state, static_cast<lua_Number>(object.sprite.alpha));
    return 1;
  }

  if (key == "name") {
    lua_rawgeti(state, LUA_REGISTRYINDEX, object.script.label);
    return 1;
  }

  if (key == "kind") {
    lua_rawgeti(state, LUA_REGISTRYINDEX, object.script.blueprint->kind);
    return 1;
  }

  lua_getfenv(state, 1);
  lua_pushvalue(state, 2);
  lua_gettable(state, -2);
  lua_remove(state, -2);
  return 1;
}

static int newindex(lua_State* state) {
  auto* self = static_cast<proxy*>(lua_touserdata(state, 1));
  std::size_t length;
  const auto* data = lua_tolstring(state, 2, &length);
  const std::string_view key{data, length};

  const auto alive = self->object != nullptr;
  assert(alive && "object must be alive when a property is written");
  [[assume(alive)]];

  auto& object = *self->object;

  if (key == "x") {
    object.sprite.x = static_cast<float>(luaL_checknumber(state, 3));
    self->dirty->mouse = true;
    return 0;
  }

  if (key == "y") {
    object.sprite.y = static_cast<float>(luaL_checknumber(state, 3));
    self->dirty->mouse = true;
    return 0;
  }

  if (key == "z") {
    const auto value = static_cast<int>(luaL_checkinteger(state, 3));
    if (object.sprite.z != value) {
      object.sprite.z = value;
      self->dirty->order = true;
      self->dirty->mouse = true;
    }

    return 0;
  }

  if (key == "mirror") {
    const auto value = std::clamp(luaL_checkinteger(state, 3), lua_Integer{}, lua_Integer{3});
    object.sprite.mirror = static_cast<mirror::value>(value);
    return 0;
  }

  if (key == "scale") {
    const auto& sequence = object.sprite.sheet->sequences[object.motion.active];
    const auto& frame = object.sprite.sheet->frames[sequence.offset + object.motion.current];
    object.sprite.resize(frame.width, frame.height, static_cast<float>(luaL_checknumber(state, 3)));
    return 0;
  }

  if (key == "angle") {
    object.sprite.angle = static_cast<float>(luaL_checknumber(state, 3));
    return 0;
  }

  if (key == "alpha") {
    object.sprite.alpha = std::clamp(static_cast<float>(luaL_checknumber(state, 3)), .0f, 255.f);
    self->dirty->mouse = true;
    return 0;
  }

  if (key == "shown") {
    object.sprite.shown = lua_toboolean(state, 3) != 0;
    self->dirty->mouse = true;
    return 0;
  }

  lua_getfenv(state, 1);
  lua_pushvalue(state, 2);
  lua_pushvalue(state, 3);
  lua_rawset(state, -3);
  lua_pop(state, 1);

  return 0;
}
}

void objects::bind(object& object, dirty& dirty, std::string_view name, std::string_view kind) {
  lua_pushlstring(L, name.data(), name.size());
  object.script.label = luaL_ref(L, LUA_REGISTRYINDEX);

  if (const auto it = prototypes.find(kind); it != prototypes.end()) [[likely]] {
    object.script.blueprint = it->second.get();
    auto* memory = static_cast<proxy*>(lua_newuserdata(L, sizeof(proxy)));
    luaL_getmetatable(L, "Object");
    lua_setmetatable(L, -2);
    lua_newtable(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, object.script.blueprint->table);
    lua_setmetatable(L, -2);
    lua_setfenv(L, -2);

    object.script.instance = luaL_ref(L, LUA_REGISTRYINDEX);
    *memory = proxy{
      .object = &object,
      .dirty = &dirty,
    };

    return;
  }

  const auto chunk = std::format("@objects/{}.lua", kind);
  const auto path = std::string_view{chunk}.substr(1);
  const auto source = io::read(path);
  if (luaL_loadbuffer(L, reinterpret_cast<const char*>(source.data()), source.size(), chunk.c_str()) != LUA_OK) [[unlikely]]
    throw std::runtime_error{lua_tostring(L, -1)};

  if (lua_pcall(L, 0, 1, 0) != LUA_OK) [[unlikely]]
    throw std::runtime_error{lua_tostring(L, -1)};

  auto blueprint = std::make_unique<prototype>();
  blueprint->table = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_pushlstring(L, kind.data(), kind.size());
  blueprint->kind = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, blueprint->table);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");

  constexpr std::array fields{
    std::pair{"on_loop", &prototype::on_loop},
    std::pair{"on_spawn", &prototype::on_spawn},
    std::pair{"on_hover", &prototype::on_hover},
    std::pair{"on_unhover", &prototype::on_unhover},
    std::pair{"on_click", &prototype::on_click},
  };

  for (const auto& [name, member] : fields) {
    lua_getfield(L, -1, name);
    blueprint.get()->*member = lua_isfunction(L, -1)
      ? luaL_ref(L, LUA_REGISTRYINDEX)
      : (lua_pop(L, 1), LUA_NOREF);
  }

  lua_pop(L, 1);

  object.script.blueprint = blueprint.get();
  prototypes.emplace(kind, std::move(blueprint));
  auto* memory = static_cast<proxy*>(lua_newuserdata(L, sizeof(proxy)));
  luaL_getmetatable(L, "Object");
  lua_setmetatable(L, -2);
  lua_newtable(L);
  lua_rawgeti(L, LUA_REGISTRYINDEX, object.script.blueprint->table);
  lua_setmetatable(L, -2);
  lua_setfenv(L, -2);

  object.script.instance = luaL_ref(L, LUA_REGISTRYINDEX);
  *memory = proxy{
    .object = &object,
    .dirty = &dirty,
  };
}

void objects::wire() {
  luaL_newmetatable(L, "Object");
  lua_pushliteral(L, "Object");
  lua_setfield(L, -2, "__name");

  lua_pushcfunction(L, index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, newindex);
  lua_setfield(L, -2, "__newindex");
  lua_pop(L, 1);
}
