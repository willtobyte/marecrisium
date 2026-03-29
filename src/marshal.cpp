#include "marshal.hpp"

namespace {
[[nodiscard]] int abs_index(lua_State *state, int index) noexcept {
  return (index > 0 || index <= LUA_REGISTRYINDEX)
    ? index
    : lua_gettop(state) + index + 1;
}

[[nodiscard]] bool lua_table_is_array(lua_State *state, int index) {
  const auto abs = abs_index(state, index);
  auto count = 0;
  lua_pushnil(state);
  while (lua_next(state, abs) != 0) {
    lua_pop(state, 1);
    ++count;
  }

  const auto length = static_cast<int>(lua_objlen(state, abs));
  return length > 0 && length == count;
}
}

void cbor_to_lua(lua_State *state, cbor_item_t *item) {
  if (cbor_is_null(item) || cbor_is_undef(item)) [[unlikely]] {
    lua_pushnil(state);
    return;
  }

  if (cbor_is_bool(item)) {
    lua_pushboolean(state, cbor_get_bool(item) ? 1 : 0);
    return;
  }

  if (cbor_isa_uint(item)) [[likely]] {
    lua_pushinteger(state, static_cast<lua_Integer>(cbor_get_int(item)));
    return;
  }

  if (cbor_isa_negint(item)) {
    lua_pushinteger(state, -1 - static_cast<lua_Integer>(cbor_get_int(item)));
    return;
  }

  if (cbor_is_float(item)) {
    lua_pushnumber(state, static_cast<lua_Number>(cbor_float_get_float(item)));
    return;
  }

  if (cbor_isa_string(item)) {
    lua_pushlstring(state,
      reinterpret_cast<const char *>(cbor_string_handle(item)),
      cbor_string_length(item));

    return;
  }

  if (cbor_isa_array(item)) {
    const auto size = cbor_array_size(item);
    lua_createtable(state, static_cast<int>(size), 0);
    auto *elements = cbor_array_handle(item);
    for (size_t i = 0; i < size; ++i) {
      cbor_to_lua(state, elements[i]);
      lua_rawseti(state, -2, static_cast<int>(i + 1));
    }

    return;
  }

  if (cbor_isa_map(item)) {
    const auto size = cbor_map_size(item);
    lua_createtable(state, 0, static_cast<int>(size));
    auto *pairs = cbor_map_handle(item);
    for (size_t i = 0; i < size; ++i) {
      cbor_to_lua(state, pairs[i].key);
      cbor_to_lua(state, pairs[i].value);
      lua_rawset(state, -3);
    }

    return;
  }

  lua_pushnil(state);
}

[[nodiscard]] cbor_item_t *lua_to_cbor(lua_State *state, int index) {
  const auto abs = abs_index(state, index);
  const auto type = lua_type(state, abs);

  switch (type) {
    case LUA_TNIL:
      return cbor_new_null();

    case LUA_TBOOLEAN:
      return cbor_build_bool(lua_toboolean(state, abs) != 0);

    case LUA_TNUMBER: {
      const auto value = lua_tonumber(state, abs);
      const auto as_int = static_cast<lua_Integer>(value);
      if (static_cast<lua_Number>(as_int) == value) {
        if (as_int >= 0)
          return cbor_build_uint64(static_cast<uint64_t>(as_int));
        return cbor_build_negint64(static_cast<uint64_t>(-1 - as_int));
      }
      return cbor_build_float8(value);
    }

    case LUA_TSTRING: {
      size_t len = 0;
      const auto *str = lua_tolstring(state, abs, &len);
      return cbor_build_stringn(str, len);
    }

    case LUA_TTABLE: {
      if (lua_table_is_array(state, abs)) [[likely]] {
        const auto length = static_cast<int>(lua_objlen(state, abs));
        auto *array = cbor_new_definite_array(static_cast<size_t>(length));
        for (auto i = 1; i <= length; ++i) {
          lua_rawgeti(state, abs, i);
          std::ignore = cbor_array_push(array, cbor_move(lua_to_cbor(state, -1)));
          lua_pop(state, 1);
        }

        return array;
      }

      auto *map = cbor_new_indefinite_map();
      lua_pushnil(state);
      while (lua_next(state, abs) != 0) {
        if (lua_type(state, -2) == LUA_TSTRING) {
          auto *key = lua_to_cbor(state, -2);
          auto *val = lua_to_cbor(state, -1);
          std::ignore = cbor_map_add(map, {cbor_move(key), cbor_move(val)});
        }
        lua_pop(state, 1);
      }

      return map;
    }

    default: [[unlikely]]
      return cbor_new_null();
  }
}

[[nodiscard]] std::vector<uint8_t> serialize(cbor_item_t *item) {
  unsigned char *buffer = nullptr;
  size_t length = 0;
  cbor_serialize_alloc(item, &buffer, &length);
  cbor_decref(&item);
  if (!buffer) [[unlikely]]
    return {};

  std::vector<uint8_t> result(buffer, buffer + length);
  free(buffer);
  return result;
}
