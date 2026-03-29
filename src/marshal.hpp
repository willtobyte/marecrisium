#pragma once

void cbor_to_lua(lua_State *state, cbor_item_t *item);
[[nodiscard]] cbor_item_t *lua_to_cbor(lua_State *state, int index);
[[nodiscard]] std::vector<uint8_t> serialize(cbor_item_t *item);
