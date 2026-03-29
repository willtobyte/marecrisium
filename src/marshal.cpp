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

void json_to_lua(lua_State *state, yyjson_val *val) {
  if (!val) [[unlikely]]
    return lua_pushnil(state);

  switch (yyjson_get_type(val)) {
    case YYJSON_TYPE_NULL:
      lua_pushnil(state);
      break;

    case YYJSON_TYPE_BOOL:
      lua_pushboolean(state, yyjson_get_bool(val) ? 1 : 0);
      break;

    case YYJSON_TYPE_NUM: {
      const auto subtype = yyjson_get_subtype(val);
      switch (subtype) {
        case YYJSON_SUBTYPE_UINT:
          lua_pushinteger(state, static_cast<lua_Integer>(yyjson_get_uint(val)));
          break;
        case YYJSON_SUBTYPE_SINT:
          lua_pushinteger(state, static_cast<lua_Integer>(yyjson_get_sint(val)));
          break;
        case YYJSON_SUBTYPE_REAL:
          lua_pushnumber(state, static_cast<lua_Number>(yyjson_get_real(val)));
          break;
        default: [[unlikely]]
          lua_pushnumber(state, static_cast<lua_Number>(yyjson_get_real(val)));
          break;
      }
    } break;

    case YYJSON_TYPE_STR:
      lua_pushlstring(state, yyjson_get_str(val), yyjson_get_len(val));
      break;

    case YYJSON_TYPE_ARR: {
      const auto size = yyjson_arr_size(val);
      lua_createtable(state, static_cast<int>(size), 0);
      yyjson_arr_iter iter;
      yyjson_arr_iter_init(val, &iter);
      yyjson_val *elem;
      auto i = 1;
      while ((elem = yyjson_arr_iter_next(&iter))) {
        json_to_lua(state, elem);
        lua_rawseti(state, -2, i);
        ++i;
      }
    } break;

    case YYJSON_TYPE_OBJ: {
      const auto size = yyjson_obj_size(val);
      lua_createtable(state, 0, static_cast<int>(size));
      yyjson_obj_iter iter;
      yyjson_obj_iter_init(val, &iter);
      yyjson_val *key;
      while ((key = yyjson_obj_iter_next(&iter))) {
        lua_pushlstring(state, yyjson_get_str(key), yyjson_get_len(key));
        json_to_lua(state, yyjson_obj_iter_get_val(key));
        lua_rawset(state, -3);
      }
    } break;

    default: [[unlikely]]
      lua_pushnil(state);
      break;
  }
}

[[nodiscard]] yyjson_mut_val *lua_to_json(lua_State *state, int index, yyjson_mut_doc *doc) {
  const auto abs = abs_index(state, index);

  switch (lua_type(state, abs)) {
    case LUA_TNIL:
      return yyjson_mut_null(doc);

    case LUA_TBOOLEAN:
      return yyjson_mut_bool(doc, lua_toboolean(state, abs) != 0);

    case LUA_TNUMBER: {
      const auto value = lua_tonumber(state, abs);
      const auto as_int = static_cast<lua_Integer>(value);
      if (static_cast<lua_Number>(as_int) == value)
        return yyjson_mut_sint(doc, as_int);
      return yyjson_mut_real(doc, static_cast<double>(value));
    }

    case LUA_TSTRING: {
      size_t len = 0;
      const auto *str = lua_tolstring(state, abs, &len);
      return yyjson_mut_strncpy(doc, str, len);
    }

    case LUA_TTABLE: {
      if (lua_table_is_array(state, abs)) [[likely]] {
        auto *arr = yyjson_mut_arr(doc);
        const auto length = static_cast<int>(lua_objlen(state, abs));
        for (auto i = 1; i <= length; ++i) {
          lua_rawgeti(state, abs, i);
          yyjson_mut_arr_append(arr, lua_to_json(state, -1, doc));
          lua_pop(state, 1);
        }
        return arr;
      }

      auto *obj = yyjson_mut_obj(doc);
      lua_pushnil(state);
      while (lua_next(state, abs) != 0) {
        size_t length = 0;
        lua_pushvalue(state, -2);
        const auto *name = lua_tolstring(state, -1, &length);
        auto *key = yyjson_mut_strncpy(doc, name, length);
        lua_pop(state, 1);
        auto *value = lua_to_json(state, -1, doc);
        yyjson_mut_obj_add(obj, key, value);
        lua_pop(state, 1);
      }
      return obj;
    }

    default: [[unlikely]]
      return yyjson_mut_null(doc);
  }
}
