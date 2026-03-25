#include "cassette.hpp"

namespace {
constexpr const char* filename = "cassette.tape";

sqlite3* database;
sqlite3_stmt* stmt_select;
sqlite3_stmt* stmt_upsert;
sqlite3_stmt* stmt_delete;
sqlite3_stmt* stmt_clear;
}

static int cassette_clear(lua_State*) {
  sqlite3_step(stmt_clear);
  sqlite3_reset(stmt_clear);

  return 0;
}

static int cassette_index(lua_State* state) {
  const auto key = std::string_view{luaL_checkstring(state, 2)};

  if (key == "clear") [[unlikely]]
    return lua_pushcfunction(state, cassette_clear), 1;

  sqlite3_bind_text(stmt_select, 1, key.data(), -1, SQLITE_STATIC);
  if (sqlite3_step(stmt_select) == SQLITE_ROW) [[likely]] {
    const auto type = sqlite3_column_int(stmt_select, 0);
    switch (type) {
      case 0:
        lua_pushboolean(state, sqlite3_column_int(stmt_select, 1) != 0 ? 1 : 0);
        break;
      case 1:
        lua_pushnumber(state, static_cast<lua_Number>(sqlite3_column_double(stmt_select, 1)));
        break;
      case 2:
        lua_pushstring(state, reinterpret_cast<const char*>(sqlite3_column_text(stmt_select, 1)));
        break;
      default:
        lua_pushnil(state);
        break;
    }
  } else {
    lua_pushnil(state);
  }

  sqlite3_reset(stmt_select);
  sqlite3_clear_bindings(stmt_select);

  return 1;
}

static int cassette_newindex(lua_State* state) {
  const auto key = std::string_view{luaL_checkstring(state, 2)};

  if (key == "clear") [[unlikely]]
    return 0;

  if (lua_isnil(state, 3)) [[unlikely]] {
    sqlite3_bind_text(stmt_delete, 1, key.data(), -1, SQLITE_STATIC);
    sqlite3_step(stmt_delete);
    sqlite3_reset(stmt_delete);
    sqlite3_clear_bindings(stmt_delete);

    return 0;
  }

  sqlite3_bind_text(stmt_upsert, 1, key.data(), -1, SQLITE_STATIC);
  if (lua_isboolean(state, 3)) {
    sqlite3_bind_int(stmt_upsert, 2, 0);
    sqlite3_bind_int(stmt_upsert, 3, lua_toboolean(state, 3));
  } else if (lua_isnumber(state, 3)) {
    sqlite3_bind_int(stmt_upsert, 2, 1);
    sqlite3_bind_double(stmt_upsert, 3, lua_tonumber(state, 3));
  } else if (lua_isstring(state, 3)) {
    sqlite3_bind_int(stmt_upsert, 2, 2);
    sqlite3_bind_text(stmt_upsert, 3, lua_tostring(state, 3), -1, SQLITE_TRANSIENT);
  } else [[unlikely]] {
    sqlite3_clear_bindings(stmt_upsert);
    return luaL_error(state, "unsupported type");
  }

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
      "type INTEGER NOT NULL,"
      "value BLOB NOT NULL"
    ")WITHOUT ROWID;",
    nullptr, nullptr, nullptr);

  sqlite3_prepare_v2(database, "SELECT type,value FROM data WHERE key=?", -1, &stmt_select, nullptr);
  sqlite3_prepare_v2(database, "INSERT OR REPLACE INTO data(key,type,value) VALUES(?,?,?)", -1, &stmt_upsert, nullptr);
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
