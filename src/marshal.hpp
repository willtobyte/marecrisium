#pragma once

namespace marshal {
  void decode(lua_State *state, yyjson_val *value);
  yyjson_mut_val *encode(lua_State *state, int index, yyjson_mut_doc *document);
}
