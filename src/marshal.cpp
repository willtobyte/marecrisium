namespace {
static bool is_sequence(lua_State *state, int table) {
  auto count = 0uz;
  lua_pushnil(state);
  while (lua_next(state, table) != 0) {
    if (lua_type(state, -2) != LUA_TNUMBER) {
      lua_pop(state, 2);
      return false;
    }

    lua_pop(state, 1);
    ++count;
  }

  const auto length = lua_objlen(state, table);
  return length > 0 && length == count;
}
}

void marshal::decode(lua_State *state, yyjson_val *value) {
  assert(value && "json value must exist");
  [[assume(value)]];

  switch (yyjson_get_type(value)) {
    case YYJSON_TYPE_NULL:
      lua_pushnil(state);
      break;

    case YYJSON_TYPE_BOOL:
      lua_pushboolean(state, yyjson_get_bool(value));
      break;

    case YYJSON_TYPE_NUM: {
      const auto subtype = yyjson_get_subtype(value);
      switch (subtype) {
        case YYJSON_SUBTYPE_UINT: {
          const auto number = yyjson_get_uint(value);
          constexpr auto maximum = static_cast<uint64_t>(std::numeric_limits<lua_Integer>::max());
          number <= maximum
            ? static_cast<void>(lua_pushinteger(state, static_cast<lua_Integer>(number)))
            : static_cast<void>(lua_pushnumber(state, static_cast<lua_Number>(number)));
        } break;
        case YYJSON_SUBTYPE_SINT:
          lua_pushinteger(state, static_cast<lua_Integer>(yyjson_get_sint(value)));
          break;
        default:
          lua_pushnumber(state, static_cast<lua_Number>(yyjson_get_real(value)));
          break;
      }
    } break;

    case YYJSON_TYPE_STR:
      lua_pushlstring(state, yyjson_get_str(value), yyjson_get_len(value));
      break;

    case YYJSON_TYPE_ARR: {
      const auto size = yyjson_arr_size(value);
      lua_createtable(state, static_cast<int>(size), 0);
      yyjson_arr_iter iterator;
      yyjson_arr_iter_init(value, &iterator);
      yyjson_val *element;
      auto index = 0;
      while ((element = yyjson_arr_iter_next(&iterator))) {
        ++index;
        decode(state, element);
        lua_rawseti(state, -2, index);
      }
    } break;

    case YYJSON_TYPE_OBJ: {
      const auto size = yyjson_obj_size(value);
      lua_createtable(state, 0, static_cast<int>(size));
      yyjson_obj_iter iterator;
      yyjson_obj_iter_init(value, &iterator);
      yyjson_val *key;
      while ((key = yyjson_obj_iter_next(&iterator))) {
        const auto *name = yyjson_get_str(key);
        const auto length = yyjson_get_len(key);
        if (length >= 2 && name[0] == '\0' && name[1] == 'n') {
          auto bits = uint64_t{};
          auto valid = length > 2 && length <= 18;
          for (auto index = 2uz; valid && index < length; ++index) {
            const auto character = static_cast<unsigned char>(name[index]);
            const auto digit = static_cast<uint64_t>(character >= '0' && character <= '9'
              ? character - '0'
              : character - 'a' + 10);
            valid = digit < 16;
            bits = bits * 16 + digit;
          }

          const auto number = std::bit_cast<lua_Number>(bits);
          valid = valid && !std::isnan(number);
          valid
            ? static_cast<void>(lua_pushnumber(state, number))
            : static_cast<void>(lua_pushlstring(state, name, length));
        } else if (length >= 2 && name[0] == '\0' && name[1] == 's') {
          lua_pushlstring(state, name + 2, length - 2);
        } else {
          lua_pushlstring(state, name, length);
        }

        decode(state, yyjson_obj_iter_get_val(key));
        lua_rawset(state, -3);
      }
    } break;

    default: [[unlikely]]
      lua_pushnil(state);
      break;
  }
}

yyjson_mut_val *marshal::encode(lua_State *state, int index, yyjson_mut_doc *document) {
  const auto source = (index > 0 || index <= LUA_REGISTRYINDEX)
    ? index
    : lua_gettop(state) + index + 1;

  switch (lua_type(state, source)) {
    case LUA_TNIL:
      return yyjson_mut_null(document);

    case LUA_TBOOLEAN:
      return yyjson_mut_bool(document, lua_toboolean(state, source) != 0);

    case LUA_TNUMBER: {
      const auto value = lua_tonumber(state, source);
      if (value == 0 && std::signbit(value))
        return yyjson_mut_real(document, value);

      constexpr auto maximum = static_cast<lua_Number>(std::numeric_limits<lua_Integer>::max());
      constexpr auto minimum = static_cast<lua_Number>(std::numeric_limits<lua_Integer>::min());
      if (value >= minimum && value < maximum) {
        const auto integer = static_cast<lua_Integer>(value);
        if (static_cast<lua_Number>(integer) == value)
          return yyjson_mut_sint(document, integer);
      }

      return yyjson_mut_real(document, value);
    }

    case LUA_TSTRING: {
      size_t length = 0;
      const auto *str = lua_tolstring(state, source, &length);
      return yyjson_mut_strn(document, str, length);
    }

    case LUA_TTABLE: {
      if (is_sequence(state, source)) [[likely]] {
        auto *array = yyjson_mut_arr(document);
        const auto size = static_cast<int>(lua_objlen(state, source));
        for (auto slot = 1; slot <= size; ++slot) {
          lua_rawgeti(state, source, slot);
          yyjson_mut_arr_append(array, encode(state, -1, document));
          lua_pop(state, 1);
        }

        return array;
      }

      auto *object = yyjson_mut_obj(document);
      lua_pushnil(state);
      while (lua_next(state, source) != 0) {
        const auto type = lua_type(state, -2);
        const auto supported = type == LUA_TSTRING || type == LUA_TNUMBER;
        assert(supported && "cassette key must be a string or number");
        [[assume(supported)]];

        yyjson_mut_val *key;
        if (type == LUA_TNUMBER) {
          std::array<char, 18> tagged{};
          tagged[1] = 'n';

          const auto bits = std::bit_cast<uint64_t>(lua_tonumber(state, -2));
          const auto result = std::to_chars(
            tagged.data() + 2, tagged.data() + tagged.size(), bits, 16);
          const auto valid = result.ec == std::errc{};
          assert(valid && "cassette numeric key must fit");
          [[assume(valid)]];
          key = yyjson_mut_strncpy(
            document, tagged.data(), static_cast<size_t>(result.ptr - tagged.data()));
        } else {
          size_t length = 0;
          const auto *name = lua_tolstring(state, -2, &length);

          if (length != 0 && name[0] == '\0') [[unlikely]] {
            auto *escaped = unsafe_yyjson_mut_str_alc(document, length + 2);
            assert(escaped && "cassette key allocation must succeed");
            [[assume(escaped)]];
            escaped[0] = '\0';
            escaped[1] = 's';
            std::memcpy(escaped + 2, name, length);
            escaped[length + 2] = '\0';
            key = yyjson_mut_strn(document, escaped, length + 2);
          } else {
            key = yyjson_mut_strn(document, name, length);
          }
        }

        auto *value = encode(state, -1, document);
        yyjson_mut_obj_add(object, key, value);
        lua_pop(state, 1);
      }

      return object;
    }

    default: [[unlikely]]
      return yyjson_mut_null(document);
  }
}
