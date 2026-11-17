/*
** luar_jit.c -- the 'jit' library (LuaJIT API surface).
**
** LuaR 0.1 ships no native code generator yet: this library is the
** stable control surface and reports its state truthfully.  jit.status()
** returns false, and luar.features.jit is false, until the tier-1
** engine lands.  The API is present so portable LuaJIT code can probe
** and fall back without crashing.
**
** See Copyright Notice in lua.h and COPYRIGHT.
*/

#define LUA_LIB
#define LUAR_LIB

#include "lprefix.h"

#include <stddef.h>
#include <string.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"
#include "luar.h"


static int jit_on (lua_State *L) {
  (void)L;    /* no native engine yet: nothing to enable */
  return 0;
}


static int jit_off (lua_State *L) {
  (void)L;
  return 0;
}


static int jit_flush (lua_State *L) {
  (void)L;
  return 0;   /* nothing compiled yet */
}


static int jit_status (lua_State *L) {
  lua_pushboolean(L, 0);   /* truthful: no native code generator in 0.1 */
  return 1;
}


static int jit_opt_start (lua_State *L) {
  /* Accept and validate the shape of LuaJIT-style option strings.  The
  ** values are kept for the future tier-1 engine in the registry.
  */
  int i;
  int n = lua_gettop(L);
  lua_newtable(L);
  for (i = 1; i <= n; i++) {
    const char *opt = luaL_checkstring(L, i);
    lua_pushstring(L, opt);
    lua_rawseti(L, -2, i);
  }
  lua_setfield(L, LUA_REGISTRYINDEX, "luar.jit.options");
  return 0;
}


static const luaL_Reg jitlib[] = {
  {"on", jit_on},
  {"off", jit_off},
  {"flush", jit_flush},
  {"status", jit_status},
  {NULL, NULL}
};


static const luaL_Reg jitoptlib[] = {
  {"start", jit_opt_start},
  {NULL, NULL}
};


LUALIB_API int luaopen_jit (lua_State *L) {
  luaL_checkversion(L);
  luaL_newlib(L, jitlib);
  lua_pushliteral(L, "LuaR " LUAR_VERSION);
  lua_setfield(L, -2, "version");
  lua_pushinteger(L, LUAR_VERSION_NUM);
  lua_setfield(L, -2, "version_num");
  lua_pushstring(L, luar_os());
  lua_setfield(L, -2, "os");
  lua_pushstring(L, luar_arch());
  lua_setfield(L, -2, "arch");
  luaL_newlib(L, jitoptlib);
  lua_setfield(L, -2, "opt");
  return 1;
}
