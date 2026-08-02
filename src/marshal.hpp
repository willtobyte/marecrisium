#pragma once

struct lua_State;
struct yyjson_mut_doc;
struct yyjson_mut_val;
struct yyjson_val;

namespace marshal {
  using table_resolver = bool (*)(lua_State *state, int index);

  void decode(lua_State *state, yyjson_val *value);
  yyjson_mut_val *encode(lua_State *state, int index, yyjson_mut_doc *document, table_resolver resolve);
}
