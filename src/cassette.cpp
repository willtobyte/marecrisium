namespace {
namespace property {
  constexpr auto purge = "purge"_hs;
}

constexpr const char *FILENAME = "cassette.tape";

sqlite3 *database;
sqlite3_stmt *stmt_select;
sqlite3_stmt *stmt_upsert;
sqlite3_stmt *stmt_delete;
sqlite3_stmt *stmt_clear;

int _purge_reference = LUA_NOREF;

struct transparent_hash final {
  using is_transparent = void;
  using is_avalanching = void;
  auto operator()(std::string_view sv) const {
    return ankerl::unordered_dense::hash<std::string_view>{}(sv);
  }
};

ankerl::unordered_dense::map<std::string, int, transparent_hash, std::equal_to<>> cache;

static void execute(sqlite3_stmt *statement) {
  [[maybe_unused]] const auto result = sqlite3_step(statement);
  assert(result == SQLITE_DONE && "sqlite3_step failed");
  sqlite3_reset(statement);
  sqlite3_clear_bindings(statement);
}

static int proxy_newindex(lua_State *state) {
  lua_pushvalue(state, lua_upvalueindex(1));
  lua_pushvalue(state, 2);
  lua_pushvalue(state, 3);
  lua_rawset(state, -3);
  lua_pop(state, 1);

  lua_getglobal(state, "cassette");
  lua_pushvalue(state, lua_upvalueindex(2));
  lua_pushvalue(state, lua_upvalueindex(3));
  lua_settable(state, -3);
  lua_pop(state, 1);
  return 0;
}

static int proxy_index(lua_State *state) {
  lua_pushvalue(state, lua_upvalueindex(1));
  lua_pushvalue(state, 2);
  lua_rawget(state, -2);

  if (lua_type(state, -1) != LUA_TTABLE) [[likely]]
    return 1;

  const auto index = lua_gettop(state);

  lua_newtable(state);
  lua_newtable(state);

  lua_pushvalue(state, index);
  lua_pushvalue(state, lua_upvalueindex(2));
  lua_pushvalue(state, lua_upvalueindex(3));
  cclosure(state, proxy_index, 3);
  lua_setfield(state, -2, "__index");

  lua_pushvalue(state, index);
  lua_pushvalue(state, lua_upvalueindex(2));
  lua_pushvalue(state, lua_upvalueindex(3));
  cclosure(state, proxy_newindex, 3);
  lua_setfield(state, -2, "__newindex");

  lua_setmetatable(state, -2);
  return 1;
}

static void proxify(lua_State *state, std::string_view key) {
  if (lua_type(state, -1) != LUA_TTABLE) [[likely]]
    return;

  const auto data = lua_gettop(state);
  lua_pushlstring(state, key.data(), key.size());
  const auto root = data + 1;

  lua_newtable(state);
  lua_newtable(state);

  lua_pushvalue(state, data);
  lua_pushvalue(state, root);
  lua_pushvalue(state, data);
  cclosure(state, proxy_index, 3);
  lua_setfield(state, -2, "__index");

  lua_pushvalue(state, data);
  lua_pushvalue(state, root);
  lua_pushvalue(state, data);
  cclosure(state, proxy_newindex, 3);
  lua_setfield(state, -2, "__newindex");

  lua_setmetatable(state, -2);

  lua_replace(state, root);
  lua_replace(state, data);
}
}

static int cassette_clear(lua_State *state) {
  for (const auto &[_, reference] : cache)
    luaL_unref(state, LUA_REGISTRYINDEX, reference);

  cache.clear();
  execute(stmt_clear);
  return 0;
}

static int cassette_index(lua_State *state) {
  const auto key = std::string_view{luaL_checkstring(state, 2)};
  const auto id = entt::hashed_string{key.data(), key.size()};

  if (id == property::purge) [[unlikely]]
    return lua_rawgeti(state, LUA_REGISTRYINDEX, _purge_reference), 1;

  if (const auto it = cache.find(key); it != cache.end()) [[likely]] {
    if (it->second == LUA_NOREF) [[unlikely]]
      return lua_pushnil(state), 1;

    lua_rawgeti(state, LUA_REGISTRYINDEX, it->second);
    proxify(state, key);
    return 1;
  }

  sqlite3_bind_text(stmt_select, 1, key.data(), static_cast<int>(key.size()), SQLITE_STATIC);

  const auto found = sqlite3_step(stmt_select) == SQLITE_ROW;
  auto *document = found
    ? yyjson_read(
        reinterpret_cast<const char *>(sqlite3_column_text(stmt_select, 0)),
        static_cast<size_t>(sqlite3_column_bytes(stmt_select, 0)),
        0)
    : nullptr;

  sqlite3_reset(stmt_select);
  sqlite3_clear_bindings(stmt_select);

  if (!document) [[unlikely]] {
    if (!found)
      cache.emplace(key, LUA_NOREF);

    return lua_pushnil(state), 1;
  }

  json_to_lua(state, yyjson_doc_get_root(document));
  yyjson_doc_free(document);

  lua_pushvalue(state, -1);
  cache.emplace(key, luaL_ref(state, LUA_REGISTRYINDEX));

  proxify(state, key);
  return 1;
}

static int cassette_newindex(lua_State *state) {
  const auto key = std::string_view{luaL_checkstring(state, 2)};
  const auto id = entt::hashed_string{key.data(), key.size()};

  if (id == property::purge) [[unlikely]]
    return 0;

  if (lua_isnil(state, 3)) [[unlikely]] {
    if (const auto it = cache.find(key); it != cache.end()) [[likely]] {
      luaL_unref(state, LUA_REGISTRYINDEX, it->second);
      cache.erase(it);
    }

    sqlite3_bind_text(stmt_delete, 1, key.data(), static_cast<int>(key.size()), SQLITE_STATIC);
    execute(stmt_delete);
    return 0;
  }

  if (const auto it = cache.find(key); it != cache.end()) {
    luaL_unref(state, LUA_REGISTRYINDEX, it->second);
    cache.erase(it);
  }

  auto *document = yyjson_mut_doc_new(nullptr);
  auto *root = lua_to_json(state, 3, document);
  yyjson_mut_doc_set_root(document, root);

  auto length = 0uz;
  auto *json = yyjson_mut_write(document, 0, &length);
  yyjson_mut_doc_free(document);

  sqlite3_bind_text(stmt_upsert, 1, key.data(), static_cast<int>(key.size()), SQLITE_STATIC);
  sqlite3_bind_text(stmt_upsert, 2, json, static_cast<int>(length), free);
  execute(stmt_upsert);
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

  cache.reserve(1024);

  cfunction(L, cassette_clear);
  _purge_reference = luaL_ref(L, LUA_REGISTRYINDEX);

  metatable(L, "Cassette", cassette_index, cassette_newindex);
  singleton(L, "Cassette", "cassette");
}
