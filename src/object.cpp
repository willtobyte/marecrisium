namespace {
std::unordered_map<std::string, prototype, transparent_string_hash, std::equal_to<>> prototypes;

keyring<
  "on_end",
  "collider",
  "x",
  "y",
  "z",
  "mirror",
  "shown",
  "scale",
  "angle",
  "alpha",
  "name",
  "kind",
  "animation"
> keys;

static int collider(lua_State* state) {
  const auto* self = static_cast<proxy*>(lua_touserdata(state, 1));
  const auto alive = self->object != nullptr;
  assert(alive && "object must be alive when its collider is read");

  const auto& object = *self->object;
  const auto& sprite = object.sprite;
  const auto& sequence = sprite.sheet->sequences[object.motion.active];
  const auto& frame = sprite.sheet->frames[sequence.offset + object.motion.current];
  const auto& bounds = sprite.bounds;

  lua_pushnumber(state, std::floor(sprite.x) + bounds.x + (frame.offset.x + frame.collider.offset.x) * sprite.scale);
  lua_pushnumber(state, std::floor(sprite.y) + bounds.y + (frame.offset.y + frame.collider.offset.y) * sprite.scale);
  lua_pushnumber(state, frame.collider.width * sprite.scale);
  lua_pushnumber(state, frame.collider.height * sprite.scale);

  return 4;
}

static int on_end_callback(lua_State* state) {
  auto* self = static_cast<proxy*>(lua_touserdata(state, 1));
  luaL_checktype(state, 2, LUA_TFUNCTION);

  auto& reference = self->object->script.on_end;
  if (reference != LUA_NOREF)
    luaL_unref(state, LUA_REGISTRYINDEX, reference);

  lua_pushvalue(state, 2);
  reference = luaL_ref(state, LUA_REGISTRYINDEX);
  self->object->motion.ending = true;

  return 0;
}

static int index(lua_State* state) {
  const auto* self = static_cast<proxy*>(lua_touserdata(state, 1));
  const auto& object = *self->object;

  switch (keys.find(lua_tostring(state, 2))) {
  case keys.id<"on_end">(): {
    lua_pushvalue(state, lua_upvalueindex(1));

    return 1;
  }

  case keys.id<"collider">(): {
    lua_pushvalue(state, lua_upvalueindex(2));

    return 1;
  }

  case keys.id<"x">(): {
    lua_pushnumber(state, static_cast<lua_Number>(object.sprite.x));

    return 1;
  }

  case keys.id<"y">(): {
    lua_pushnumber(state, static_cast<lua_Number>(object.sprite.y));

    return 1;
  }

  case keys.id<"z">(): {
    lua_pushinteger(state, static_cast<lua_Integer>(object.sprite.z));

    return 1;
  }

  case keys.id<"mirror">(): {
    lua_pushinteger(state, std::to_underlying(object.sprite.mirror));

    return 1;
  }

  case keys.id<"shown">(): {
    lua_pushboolean(state, object.sprite.shown);

    return 1;
  }

  case keys.id<"scale">(): {
    lua_pushnumber(state, static_cast<lua_Number>(object.sprite.scale));

    return 1;
  }

  case keys.id<"angle">(): {
    lua_pushnumber(state, static_cast<lua_Number>(object.sprite.angle));

    return 1;
  }

  case keys.id<"alpha">(): {
    lua_pushnumber(state, static_cast<lua_Number>(object.sprite.alpha));

    return 1;
  }

  case keys.id<"name">(): {
    lua_rawgeti(state, LUA_REGISTRYINDEX, object.script.label);

    return 1;
  }

  case keys.id<"kind">(): {
    lua_rawgeti(state, LUA_REGISTRYINDEX, object.script.blueprint->kind);

    return 1;
  }

  case keys.id<"animation">(): {
    const auto& sequence = object.sprite.sheet->sequences[object.motion.active];
    lua_rawgeti(state, LUA_REGISTRYINDEX, sequence.name);

    return 1;
  }

  default:
    break;
  }

  lua_getfenv(state, 1);
  lua_pushvalue(state, 2);
  lua_gettable(state, -2);
  lua_remove(state, -2);

  return 1;
}

static int newindex(lua_State* state) {
  auto* self = static_cast<proxy*>(lua_touserdata(state, 1));
  auto& object = *self->object;

  switch (keys.find(lua_tostring(state, 2))) {
  case keys.id<"x">(): {
    object.sprite.x = static_cast<float>(luaL_checknumber(state, 3));

    return 0;
  }

  case keys.id<"y">(): {
    object.sprite.y = static_cast<float>(luaL_checknumber(state, 3));

    return 0;
  }

  case keys.id<"z">(): {
    const auto value = static_cast<int>(luaL_checkinteger(state, 3));
    if (object.sprite.z != value) {
      object.sprite.z = value;
      self->dirty->order = true;
    }

    return 0;
  }

  case keys.id<"mirror">(): {
    const auto value = std::clamp(luaL_checkinteger(state, 3), lua_Integer{}, lua_Integer{3});
    object.sprite.mirror = static_cast<mirror::value>(value);

    return 0;
  }

  case keys.id<"scale">(): {
    const auto* sheet = object.sprite.sheet;
    const auto& source = sheet->source;
    object.sprite.resize(source.width, source.height, static_cast<float>(luaL_checknumber(state, 3)));

    return 0;
  }

  case keys.id<"angle">(): {
    object.sprite.angle = static_cast<float>(luaL_checknumber(state, 3));

    return 0;
  }

  case keys.id<"alpha">(): {
    object.sprite.alpha = static_cast<uint8_t>(std::clamp(luaL_checknumber(state, 3), lua_Number{}, static_cast<lua_Number>(255)));

    return 0;
  }

  case keys.id<"shown">(): {
    object.sprite.shown = lua_toboolean(state, 3) != 0;

    return 0;
  }

  case keys.id<"animation">(): {
    std::size_t length;
    const auto* data = luaL_checklstring(state, 3, &length);
    const auto* sheet = object.sprite.sheet;
    const auto it = sheet->lookup->find({data, length});
    const bool known = it != sheet->lookup->end();
    assert(known && "unknown animation");
    [[assume(known)]];

    object.motion.active = it->second;
    object.motion.current = 0;
    object.motion.elapsed = 0;

    return 0;
  }

  default:
    break;
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
    object.script.blueprint = &it->second;
    auto* memory = static_cast<proxy*>(lua_newuserdata(L, sizeof(proxy)));
    luaL_getmetatable(L, "Object");
    lua_setmetatable(L, -2);
    lua_createtable(L, 0, 0);
    lua_rawgeti(L, LUA_REGISTRYINDEX, object.script.blueprint->table);
    lua_setmetatable(L, -2);
    lua_setfenv(L, -2);

    object.script.instance = luaL_ref(L, LUA_REGISTRYINDEX);
    *memory = {
      .object = &object,
      .dirty = &dirty,
    };

    return;
  }

  const auto chunk = std::format("@objects/{}.lua", kind);
  const auto filename = std::string_view{chunk}.substr(1);
  const auto source = io::read(filename);
  if (luaL_loadbuffer(L, reinterpret_cast<const char*>(source.data()), source.size(), chunk.c_str()) != LUA_OK) [[unlikely]]
    throw std::runtime_error{lua_tostring(L, -1)};

  if (pcall(L, 0, 1) != LUA_OK) [[unlikely]]
    throw std::runtime_error{lua_tostring(L, -1)};

  prototype blueprint;
  blueprint.table = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_pushlstring(L, kind.data(), kind.size());
  blueprint.kind = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, blueprint.table);
  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");

  constexpr std::array fields{
    std::pair{"on_loop", &prototype::on_loop},
    std::pair{"on_spawn", &prototype::on_spawn},
  };

  for (const auto& [name, member] : fields) {
    lua_getfield(L, -1, name);
    blueprint.*member = lua_isfunction(L, -1)
      ? luaL_ref(L, LUA_REGISTRYINDEX)
      : (lua_pop(L, 1), LUA_NOREF);
  }

  lua_pop(L, 1);

  object.script.blueprint = &prototypes.emplace(kind, std::move(blueprint)).first->second;
  auto* memory = static_cast<proxy*>(lua_newuserdata(L, sizeof(proxy)));
  luaL_getmetatable(L, "Object");
  lua_setmetatable(L, -2);
  lua_createtable(L, 0, 0);
  lua_rawgeti(L, LUA_REGISTRYINDEX, object.script.blueprint->table);
  lua_setmetatable(L, -2);
  lua_setfenv(L, -2);

  object.script.instance = luaL_ref(L, LUA_REGISTRYINDEX);
  *memory = {
    .object = &object,
    .dirty = &dirty,
  };
}

void objects::wire() {
  lua_createtable(L, 0, 3);
  lua_pushliteral(L, "Object");
  lua_setfield(L, -2, "__name");

  keys.intern();

  lua_pushcfunction(L, on_end_callback);
  lua_pushcfunction(L, collider);
  lua_pushcclosure(L, index, 2);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, newindex);
  lua_setfield(L, -2, "__newindex");
  lua_setfield(L, LUA_REGISTRYINDEX, "Object");
}
