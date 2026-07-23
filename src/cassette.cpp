namespace {
namespace lookup {
  constexpr auto purge = "purge"_hs;
}

constexpr const char *FILENAME = "cassette.tape";

sqlite3 *database;
sqlite3_stmt *stmt_select;
sqlite3_stmt *stmt_upsert;
sqlite3_stmt *stmt_delete;
sqlite3_stmt *stmt_clear;

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
  sqlite3_bind_text(stmt_select, 1, key.data(), static_cast<int>(key.size()), SQLITE_STATIC);
  const auto found = sqlite3_step(stmt_select) == SQLITE_ROW;

  if (found)
    lua_pushlstring(state,
      reinterpret_cast<const char *>(sqlite3_column_text(stmt_select, 0)),
      static_cast<size_t>(sqlite3_column_bytes(stmt_select, 0)));

  sqlite3_reset(stmt_select);
  sqlite3_clear_bindings(stmt_select);
  return found;
}

static void save(lua_State *state, std::string_view key, int index) {
  auto *document = yyjson_mut_doc_new(nullptr);
  auto *root = lua_to_json(state, index, document);
  yyjson_mut_doc_set_root(document, root);

  auto length = 0uz;
  auto *json = yyjson_mut_write(document, 0, &length);
  yyjson_mut_doc_free(document);

  lua_pushlstring(state, json, length);
  free(json);

  sqlite3_bind_text(stmt_upsert, 1, key.data(), static_cast<int>(key.size()), SQLITE_STATIC);
  sqlite3_bind_text(stmt_upsert, 2, lua_tostring(state, -1), static_cast<int>(length), SQLITE_STATIC);
  execute(stmt_upsert);
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
  lua_newtable(state);

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

  lua_setmetatable(state, -2);
}

}

static int cassette_clear(lua_State *state) {
  execute(stmt_clear);
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
  auto *document = yyjson_read(json, length, 0);

  json_to_lua(state, yyjson_doc_get_root(document));
  yyjson_doc_free(document);

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
    sqlite3_bind_text(stmt_delete, 1, key.data(), static_cast<int>(key.size()), SQLITE_STATIC);
    execute(stmt_delete);
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
  sqlite3_open(FILENAME, &database);
  sqlite3_exec(database,
    "PRAGMA journal_mode=WAL;"
    "PRAGMA synchronous=NORMAL;"
    "PRAGMA temp_store=MEMORY;"
    "PRAGMA mmap_size=67108864;"
    "CREATE TABLE IF NOT EXISTS data("
      "key TEXT PRIMARY KEY,"
      "value JSONB NOT NULL"
    ")WITHOUT ROWID;",
    nullptr, nullptr, nullptr);

  sqlite3_prepare_v2(database, "SELECT json(value) FROM data WHERE key=?", -1, &stmt_select, nullptr);
  sqlite3_prepare_v2(database, "INSERT OR REPLACE INTO data(key,value) VALUES(?,jsonb(?))", -1, &stmt_upsert, nullptr);
  sqlite3_prepare_v2(database, "DELETE FROM data WHERE key=?", -1, &stmt_delete, nullptr);
  sqlite3_prepare_v2(database, "DELETE FROM data", -1, &stmt_clear, nullptr);

  std::atexit(+[] {
    sqlite3_finalize(stmt_select);
    sqlite3_finalize(stmt_upsert);
    sqlite3_finalize(stmt_delete);
    sqlite3_finalize(stmt_clear);
    sqlite3_close(database);
  });

  cfunction(L, cassette_clear);
  _purge_reference = luaL_ref(L, LUA_REGISTRYINDEX);

  metatable(L, "Cassette", cassette_index, cassette_newindex);
  singleton(L, "Cassette", "cassette");
}
