#pragma once

void json_to_lua(lua_State *state, yyjson_val *val);
[[nodiscard]] yyjson_mut_val *lua_to_json(lua_State *state, int index, yyjson_mut_doc *doc);
