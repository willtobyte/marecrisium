#pragma once

namespace error {
[[noreturn]] void raise(lua_State* state);
}
