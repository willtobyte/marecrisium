namespace {
constexpr const char *FILENAME = "cassette.tape";

std::unique_ptr<sqlite3, SQLite_Database_Deleter> database;
std::unique_ptr<sqlite3_stmt, SQLite_Statement_Deleter> stmt_select;
std::unique_ptr<sqlite3_stmt, SQLite_Statement_Deleter> stmt_upsert;
std::unique_ptr<sqlite3_stmt, SQLite_Statement_Deleter> stmt_delete;
std::unique_ptr<sqlite3_stmt, SQLite_Statement_Deleter> stmt_clear;

char proxy_data;

static bool resolve_proxy(lua_State *state, int table) {
  lua_pushlightuserdata(state, &proxy_data);
  lua_rawget(state, table);

  if (lua_istable(state, -1))
    return true;

  lua_pop(state, 1);
  return false;
}

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
  const auto document = std::unique_ptr<yyjson_mut_doc, YYJSON_Mut_Doc_Deleter>{yyjson_mut_doc_new(nullptr)};
  auto *root = marshal::encode(state, index, document.get(), resolve_proxy);
  yyjson_mut_doc_set_root(document.get(), root);

  auto length = 0uz;
  const auto json = std::unique_ptr<char, STD_Deleter>{yyjson_mut_write(document.get(), 0, &length)};
  const auto *data = json.get();
  assert(data && "cassette value must be valid JSON");
  [[assume(data)]];

  auto *statement = stmt_upsert.get();
  sqlite3_bind_text(statement, 1, key.data(), static_cast<int>(key.size()), SQLITE_STATIC);
  sqlite3_bind_text(statement, 2, data, static_cast<int>(length), SQLITE_STATIC);
  execute(statement);
}

static int proxy_newindex(lua_State *state) {
  auto length = 0uz;
  const auto key = std::string_view{lua_tolstring(state, lua_upvalueindex(2), &length), length};

  lua_pushvalue(state, lua_upvalueindex(1));
  lua_pushvalue(state, 2);
  lua_pushvalue(state, 3);
  lua_rawset(state, -3);
  lua_pop(state, 1);

  save(state, key, lua_upvalueindex(3));
  return 0;
}

static int length_callback(lua_State *state) {
  lua_pushlightuserdata(state, &proxy_data);
  lua_rawget(state, 1);
  lua_pushinteger(state, static_cast<lua_Integer>(lua_objlen(state, -1)));
  return 1;
}

static int iterate_callback(lua_State *state) {
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

static int pairs_callback(lua_State *state) {
  lua_pushlightuserdata(state, &proxy_data);
  lua_rawget(state, 1);
  lua_pushvalue(state, -1);
  lua_pushboolean(state, false);
  lua_pushcclosure(state, iterate_callback, 2);
  lua_pushvalue(state, 1);
  lua_pushnil(state);
  return 3;
}

static int ipairs_callback(lua_State *state) {
  lua_pushlightuserdata(state, &proxy_data);
  lua_rawget(state, 1);
  lua_pushvalue(state, -1);
  lua_pushboolean(state, true);
  lua_pushcclosure(state, iterate_callback, 2);
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
  if (resolve_proxy(state, index))
    lua_replace(state, index);

  proxify(state, index, lua_upvalueindex(2), lua_upvalueindex(3));
  return 1;
}

static void proxify(lua_State *state, int data, int key, int root) {
  lua_newtable(state);
  lua_pushlightuserdata(state, &proxy_data);
  lua_pushvalue(state, data);
  lua_rawset(state, -3);

  lua_createtable(state, 0, 5);
  lua_pushvalue(state, data);
  lua_pushvalue(state, key);
  lua_pushvalue(state, root);
  lua_pushcclosure(state, proxy_index, 3);
  lua_setfield(state, -2, "__index");

  lua_pushvalue(state, data);
  lua_pushvalue(state, key);
  lua_pushvalue(state, root);
  lua_pushcclosure(state, proxy_newindex, 3);
  lua_setfield(state, -2, "__newindex");

  lua_pushcfunction(state, length_callback);
  lua_setfield(state, -2, "__len");

  lua_pushcfunction(state, pairs_callback);
  lua_setfield(state, -2, "__pairs");

  lua_pushcfunction(state, ipairs_callback);
  lua_setfield(state, -2, "__ipairs");

  lua_setmetatable(state, -2);
}

}

static int purge_callback(lua_State*) {
  execute(stmt_clear.get());
  return 0;
}

static int index(lua_State *state) {
  auto size = 0uz;
  const auto* name = luaL_checklstring(state, 2, &size);
  const auto key = std::string_view{name, size};

  if (key == "purge") {
    lua_pushcfunction(state, purge_callback);
    return 1;
  }

  if (load(state, key)) [[likely]] {
    auto length = 0uz;
    const auto *json = lua_tolstring(state, -1, &length);
    const auto document = std::unique_ptr<yyjson_doc, YYJSON_Doc_Deleter>{yyjson_read(json, length, 0)};

    marshal::decode(state, yyjson_doc_get_root(document.get()));

    if (lua_type(state, -1) == LUA_TTABLE) {
      const auto data = lua_gettop(state);

      lua_pushlstring(state, key.data(), key.size());
      const auto root = data + 1;
      proxify(state, data, root, data);

      lua_replace(state, root);
      lua_replace(state, data);
    }

    return 1;
  }

  lua_pushnil(state);
  return 1;
}

static int newindex(lua_State *state) {
  auto size = 0uz;
  const auto* name = luaL_checklstring(state, 2, &size);
  const auto key = std::string_view{name, size};

  if (lua_isnil(state, 3)) [[unlikely]] {
    sqlite3_bind_text(stmt_delete.get(), 1, key.data(), static_cast<int>(key.size()), SQLITE_STATIC);
    execute(stmt_delete.get());
    return 0;
  }

  save(state, key, 3);
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
      "value TEXT NOT NULL"
    ")WITHOUT ROWID;",
    nullptr, nullptr, nullptr);

  sqlite3_prepare_v2(handle, "SELECT value FROM data WHERE key=?", -1, std::out_ptr(stmt_select), nullptr);
  sqlite3_prepare_v2(handle, "INSERT OR REPLACE INTO data(key,value) VALUES(?,?)", -1, std::out_ptr(stmt_upsert), nullptr);
  sqlite3_prepare_v2(handle, "DELETE FROM data WHERE key=?", -1, std::out_ptr(stmt_delete), nullptr);
  sqlite3_prepare_v2(handle, "DELETE FROM data", -1, std::out_ptr(stmt_clear), nullptr);

  std::atexit(+[] {
    stmt_select.reset();
    stmt_upsert.reset();
    stmt_delete.reset();
    stmt_clear.reset();
    database.reset();
  });

  lua_newtable(L);
  lua_pushcfunction(L, index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, newindex);
  lua_setfield(L, -2, "__newindex");
  lua_setmetatable(L, -2);
  lua_setglobal(L, "cassette");
}
