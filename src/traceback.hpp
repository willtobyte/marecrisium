#pragma once

int traceback(lua_State* state);
int pcall(lua_State* state, int args, int results);

[[noreturn]] void propagate();
