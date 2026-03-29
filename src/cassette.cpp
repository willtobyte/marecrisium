#include "cassette.hpp"

namespace {
constexpr const char *filename = "cassette.tape";

sqlite3 *database;
sqlite3_stmt *stmt_select;
sqlite3_stmt *stmt_upsert;
sqlite3_stmt *stmt_delete;
sqlite3_stmt *stmt_clear;

std::unordered_map<std::string, int, transparent_hash, std::equal_to<>> cache;
}

static int cassette_clear(lua_State *state) {
  for (auto &[_, ref] : cache)
    luaL_unref(state, LUA_REGISTRYINDEX, ref);

  cache.clear();

  sqlite3_step(stmt_clear);
  sqlite3_reset(stmt_clear);

  return 0;
}

static int cassette_index(lua_State *state) {
  const auto key = std::string_view{luaL_checkstring(state, 2)};

  if (key == "clear") [[unlikely]]
    return lua_pushcfunction(state, cassette_clear), 1;

  if (const auto it = cache.find(key); it != cache.end()) [[likely]] {
    lua_rawgeti(state, LUA_REGISTRYINDEX, it->second);
    return 1;
  }

  sqlite3_bind_text(stmt_select, 1, key.data(), static_cast<int>(key.size()), SQLITE_STATIC);
  if (sqlite3_step(stmt_select) == SQLITE_ROW) [[likely]] {
    const auto *json = reinterpret_cast<const char *>(sqlite3_column_text(stmt_select, 0));
    const auto length = static_cast<size_t>(sqlite3_column_bytes(stmt_select, 0));

    auto *document = yyjson_read(json, length, 0);
    if (!document) [[unlikely]] {
      lua_pushnil(state);
    } else {
      json_to_lua(state, yyjson_doc_get_root(document));
      yyjson_doc_free(document);

      lua_pushvalue(state, -1);
      cache.emplace(std::string{key}, luaL_ref(state, LUA_REGISTRYINDEX));
    }
  } else {
    lua_pushnil(state);
  }

  sqlite3_reset(stmt_select);
  sqlite3_clear_bindings(stmt_select);

  return 1;
}

static int cassette_newindex(lua_State *state) {
  const auto key = std::string_view{luaL_checkstring(state, 2)};

  if (key == "clear") [[unlikely]]
    return 0;

  if (lua_isnil(state, 3)) [[unlikely]] {
    if (const auto it = cache.find(key); it != cache.end()) [[unlikely]] {
      luaL_unref(state, LUA_REGISTRYINDEX, it->second);
      cache.erase(it);
    }

    sqlite3_bind_text(stmt_delete, 1, key.data(), static_cast<int>(key.size()), SQLITE_STATIC);
    sqlite3_step(stmt_delete);
    sqlite3_reset(stmt_delete);
    sqlite3_clear_bindings(stmt_delete);

    return 0;
  }

  lua_pushvalue(state, 3);
  const auto reference = luaL_ref(state, LUA_REGISTRYINDEX);

  auto [it, inserted] = cache.try_emplace(std::string{key}, reference);
  if (!inserted) [[likely]] {
    luaL_unref(state, LUA_REGISTRYINDEX, it->second);
    it->second = reference;
  }

  auto *document = yyjson_mut_doc_new(nullptr);
  auto *root = lua_to_json(state, 3, document);
  yyjson_mut_doc_set_root(document, root);

  size_t length = 0;
  auto *json = yyjson_mut_write(document, 0, &length);
  yyjson_mut_doc_free(document);

  sqlite3_bind_text(stmt_upsert, 1, key.data(), static_cast<int>(key.size()), SQLITE_STATIC);
  sqlite3_bind_text(stmt_upsert, 2, json, static_cast<int>(length), free);
  sqlite3_step(stmt_upsert);
  sqlite3_reset(stmt_upsert);
  sqlite3_clear_bindings(stmt_upsert);

  return 0;
}

void cassette::wire() {
  sqlite3_open(filename, &database);
  sqlite3_exec(database,
    "PRAGMA journal_mode=WAL;"
    "PRAGMA synchronous=FULL;"
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

  metatable(L, "Cassette", cassette_index, cassette_newindex);

  singleton(L, "Cassette", "cassette");
}
