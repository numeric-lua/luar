/* tests/embed/module.c -- minimal loadable C module for shared ABI tests. */
#include "lua.h"
#include "lauxlib.h"

static int answer (lua_State *L) {
  lua_pushinteger(L, 42);
  return 1;
}

LUAMOD_API int luaopen_luartestmod (lua_State *L) {
  static const luaL_Reg funcs[] = {
    {"answer", answer},
    {NULL, NULL}
  };
  luaL_newlib(L, funcs);
  return 1;
}
