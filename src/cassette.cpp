namespace {
namespace lookup {
  constexpr auto purge = "purge"_hs;
}

constexpr const char *FILENAME = "cassette.tape";

struct json_deleter final {
  void operator()(yyjson_doc *document) const {
    yyjson_doc_free(document);
  }

  void operator()(yyjson_mut_doc *document) const {
    yyjson_mut_doc_free(document);
  }

  void operator()(char *json) const {
    std::free(json);
  }
};

struct sqlite_deleter final {
  void operator()(sqlite3 *handle) const {
    sqlite3_close(handle);
  }

  void operator()(sqlite3_stmt *statement) const {
    sqlite3_finalize(statement);
  }
};

std::unique_ptr<sqlite3, sqlite_deleter> database;
std::unique_ptr<sqlite3_stmt, sqlite_deleter> stmt_select;
std::unique_ptr<sqlite3_stmt, sqlite_deleter> stmt_upsert;
std::unique_ptr<sqlite3_stmt, sqlite_deleter> stmt_delete;
std::unique_ptr<sqlite3_stmt, sqlite_deleter> stmt_clear;

int _purge_reference = LUA_NOREF;

char holder;
char token;

static void execute(sqlite3_stmt *statement) {
  [[maybe_unused]] const auto result = sqlite3_step(statement);
  assert(result == SQLITE_DONE && "sqlite3_step failed");
  sqlite3_reset(statement);
  sqlite3_clear_bindings(statement);
}

static auto load(lua_State *state, std::string_view key) {
  auto *statement = stmt_select.get();
  sqlite3_bind_text(statement, 1, key.data(), static_cast<int>(key.size()), SQLITE_STATIC);
  const auto found = sqlite3_step(statement) == SQLITE_ROW;

  if (found)
    lua_pushlstring(state,
      reinterpret_cast<const char *>(sqlite3_column_text(statement, 0)),
      static_cast<size_t>(sqlite3_column_bytes(statement, 0)));

  sqlite3_reset(statement);
  sqlite3_clear_bindings(statement);
  return found;
}

static void save(lua_State *state, std::string_view key, int index) {
  const auto document = std::unique_ptr<yyjson_mut_doc, json_deleter>{yyjson_mut_doc_new(nullptr)};
  auto *root = marshal::encode(state, index, document.get());
  yyjson_mut_doc_set_root(document.get(), root);

  auto length = 0uz;
  const auto json = std::unique_ptr<char, json_deleter>{yyjson_mut_write(document.get(), 0, &length)};

  lua_pushlstring(state, json.get(), length);

  auto *statement = stmt_upsert.get();
  sqlite3_bind_text(statement, 1, key.data(), static_cast<int>(key.size()), SQLITE_STATIC);
  sqlite3_bind_text(statement, 2, lua_tostring(state, -1), static_cast<int>(length), SQLITE_STATIC);
  execute(statement);
}

static int proxy_newindex(lua_State *state) {
  auto length = 0uz;
  const auto key = std::string_view{lua_tolstring(state, lua_upvalueindex(2), &length), length};

  if (!load(state, key)) [[unlikely]]
    return 0;

  lua_getmetatable(state, lua_upvalueindex(3));
  lua_pushlightuserdata(state, &token);
  lua_rawget(state, -2);
  lua_remove(state, -2);
  const auto current = lua_rawequal(state, -1, -2) != 0;
  lua_pop(state, 2);

  if (!current) [[unlikely]]
    return 0;

  lua_pushvalue(state, lua_upvalueindex(1));
  lua_pushvalue(state, 2);
  lua_pushvalue(state, 3);
  lua_rawset(state, -3);
  lua_pop(state, 1);

  save(state, key, lua_upvalueindex(3));
  const auto snapshot = lua_gettop(state);

  lua_getmetatable(state, lua_upvalueindex(3));
  lua_pushlightuserdata(state, &token);
  lua_pushvalue(state, snapshot);
  lua_rawset(state, -3);
  lua_pop(state, 1);
  return 0;
}

static int length(lua_State *state) {
  lua_getmetatable(state, 1);
  lua_pushlightuserdata(state, &holder);
  lua_rawget(state, -2);
  lua_remove(state, -2);
  lua_pushinteger(state, static_cast<lua_Integer>(lua_objlen(state, -1)));
  return 1;
}

static int iterate(lua_State *state) {
  if (lua_toboolean(state, lua_upvalueindex(2)) == 0) {
    lua_pushvalue(state, lua_upvalueindex(1));
    lua_pushvalue(state, 2);

    if (lua_next(state, -2) == 0)
      return 0;

    lua_pop(state, 1);
    lua_pushvalue(state, 1);
    lua_pushvalue(state, -2);
    lua_gettable(state, -2);
    lua_remove(state, -2);
    return 2;
  }

  const auto index = lua_tointeger(state, 2) + 1;
  lua_pushinteger(state, index);
  lua_pushvalue(state, 1);
  lua_pushinteger(state, index);
  lua_gettable(state, -2);
  lua_remove(state, -2);

  if (lua_isnil(state, -1))
    return 0;

  return 2;
}

static int pairs(lua_State *state) {
  lua_getmetatable(state, 1);
  lua_pushlightuserdata(state, &holder);
  lua_rawget(state, -2);
  lua_remove(state, -2);
  lua_pushvalue(state, -1);
  lua_pushboolean(state, false);
  cclosure(state, iterate, 2);
  lua_pushvalue(state, 1);
  lua_pushnil(state);
  return 3;
}

static int ipairs(lua_State *state) {
  lua_getmetatable(state, 1);
  lua_pushlightuserdata(state, &holder);
  lua_rawget(state, -2);
  lua_remove(state, -2);
  lua_pushvalue(state, -1);
  lua_pushboolean(state, true);
  cclosure(state, iterate, 2);
  lua_pushvalue(state, 1);
  lua_pushinteger(state, 0);
  return 3;
}

static void proxify(lua_State *state, int data, int key, int root);

static int proxy_index(lua_State *state) {
  lua_pushvalue(state, lua_upvalueindex(1));
  lua_pushvalue(state, 2);
  lua_rawget(state, -2);

  if (lua_type(state, -1) != LUA_TTABLE) [[likely]]
    return 1;

  const auto index = lua_gettop(state);
  proxify(state, index, lua_upvalueindex(2), lua_upvalueindex(3));
  return 1;
}

static void proxify(lua_State *state, int data, int key, int root) {
  lua_newtable(state);
  lua_createtable(state, 0, 6);

  lua_pushlightuserdata(state, &holder);
  lua_pushvalue(state, data);
  lua_rawset(state, -3);

  lua_pushvalue(state, data);
  lua_pushvalue(state, key);
  lua_pushvalue(state, root);
  cclosure(state, proxy_index, 3);
  lua_setfield(state, -2, "__index");

  lua_pushvalue(state, data);
  lua_pushvalue(state, key);
  lua_pushvalue(state, root);
  cclosure(state, proxy_newindex, 3);
  lua_setfield(state, -2, "__newindex");

  lua_pushcfunction(state, length);
  lua_setfield(state, -2, "__len");

  lua_pushcfunction(state, pairs);
  lua_setfield(state, -2, "__pairs");

  lua_pushcfunction(state, ipairs);
  lua_setfield(state, -2, "__ipairs");

  lua_setmetatable(state, -2);
}

}

static int cassette_clear(lua_State *state) {
  execute(stmt_clear.get());
  return 0;
}

static int cassette_index(lua_State *state) {
  const auto key = std::string_view{luaL_checkstring(state, 2)};
  const auto id = entt::hashed_string{key.data(), key.size()};

  if (id == lookup::purge) [[unlikely]]
    return lua_rawgeti(state, LUA_REGISTRYINDEX, _purge_reference), 1;

  if (!load(state, key)) [[unlikely]]
    return lua_pushnil(state), 1;

  auto length = 0uz;
  const auto *json = lua_tolstring(state, -1, &length);
  const auto document = std::unique_ptr<yyjson_doc, json_deleter>{yyjson_read(json, length, 0)};

  marshal::decode(state, yyjson_doc_get_root(document.get()));

  if (lua_type(state, -1) == LUA_TTABLE) {
    const auto data = lua_gettop(state);
    const auto snapshot = data - 1;

    lua_newtable(state);
    lua_pushlightuserdata(state, &token);
    lua_pushvalue(state, snapshot);
    lua_rawset(state, -3);
    lua_setmetatable(state, data);

    lua_pushlstring(state, key.data(), key.size());
    const auto root = data + 1;
    proxify(state, data, root, data);

    lua_replace(state, root);
    lua_replace(state, data);
  }

  return 1;
}

static int cassette_newindex(lua_State *state) {
  const auto key = std::string_view{luaL_checkstring(state, 2)};
  const auto id = entt::hashed_string{key.data(), key.size()};
  auto value = 3;

  if (id == lookup::purge) [[unlikely]]
    return 0;

  if (lua_isnil(state, 3)) [[unlikely]] {
    sqlite3_bind_text(stmt_delete.get(), 1, key.data(), static_cast<int>(key.size()), SQLITE_STATIC);
    execute(stmt_delete.get());
    return 0;
  }

  if (lua_getmetatable(state, value)) {
    lua_pushlightuserdata(state, &holder);
    lua_rawget(state, -2);
    lua_remove(state, -2);

    if (lua_istable(state, -1))
      value = lua_gettop(state);
    else
      lua_pop(state, 1);
  }

  save(state, key, value);
  lua_pop(state, 1);
  return 0;
}

void cassette::wire() {
  sqlite3_open(FILENAME, std::out_ptr(database));
  auto *handle = database.get();
  sqlite3_exec(handle,
    "PRAGMA journal_mode=WAL;"
    "PRAGMA synchronous=NORMAL;"
    "PRAGMA temp_store=MEMORY;"
    "PRAGMA mmap_size=67108864;"
    "CREATE TABLE IF NOT EXISTS data("
      "key TEXT PRIMARY KEY,"
      "value JSONB NOT NULL"
    ")WITHOUT ROWID;",
    nullptr, nullptr, nullptr);

  sqlite3_prepare_v2(handle, "SELECT json(value) FROM data WHERE key=?", -1, std::out_ptr(stmt_select), nullptr);
  sqlite3_prepare_v2(handle, "INSERT OR REPLACE INTO data(key,value) VALUES(?,jsonb(?))", -1, std::out_ptr(stmt_upsert), nullptr);
  sqlite3_prepare_v2(handle, "DELETE FROM data WHERE key=?", -1, std::out_ptr(stmt_delete), nullptr);
  sqlite3_prepare_v2(handle, "DELETE FROM data", -1, std::out_ptr(stmt_clear), nullptr);

  std::atexit(+[] {
    stmt_select.reset();
    stmt_upsert.reset();
    stmt_delete.reset();
    stmt_clear.reset();
    database.reset();
  });

  cfunction(L, cassette_clear);
  _purge_reference = luaL_ref(L, LUA_REGISTRYINDEX);

  metatable(L, "Cassette", cassette_index, cassette_newindex);
  singleton(L, "Cassette", "cassette");
}
