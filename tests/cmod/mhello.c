/*
** mhello.c -- a stock Lua 5.5 C module, built out-of-tree, used to prove
** the module ABI claim: luar exports the Lua 5.5 C API, so modules built
** against the system lua5.5 headers load and run unchanged.
*/

#include <lua.h>
#include <lauxlib.h>

static int hello (lua_State *L) {
  lua_pushstring(L, "hello from a stock lua5.5 module");
  lua_pushinteger(L, (lua_Integer)lua_version(L));
  return 2;
}

static const luaL_Reg funcs[] = {
  {"hello", hello},
  {NULL, NULL}
};

int luaopen_mhello (lua_State *L) {
  luaL_newlib(L, funcs);
  return 1;
}
