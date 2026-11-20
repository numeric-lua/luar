/*
** embed.c -- a minimal C host embedding Luar through the public headers.
**
** Proves that luar.h/libluar.a work for embedders and that luaL_openlibs
** brings up the Luar extensions (luar, jit, bit, and ffi when built).
*/

#include <stdio.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "luar.h"

static int fail (const char *msg) {
  fprintf(stderr, "embed: FAIL: %s\n", msg);
  return 1;
}

int main (void) {
  lua_State *L = luaL_newstate();
  if (L == NULL) return fail("luaL_newstate");

  if (strcmp(luar_version(), "0.50.1") != 0) return fail("luar_version");
  if (strcmp(luar_lua_version(), "Lua 5.5.1") != 0) return fail("luar_lua_version");

  luaL_openlibs(L);

  if (lua_getglobal(L, "luar") != LUA_TTABLE)
    return fail("luar table missing after luaL_openlibs");
  lua_getfield(L, -1, "version");
  if (strcmp(lua_tostring(L, -1), "0.50.1") != 0) return fail("luar.version");
  lua_pop(L, 2);   /* version, luar */

  lua_getglobal(L, "jit");
  if (!lua_istable(L, -1)) return fail("jit table missing");
  lua_getfield(L, -1, "status");
  if (lua_pcall(L, 0, 1, 0) != LUA_OK) return fail("jit.status");
  if (!lua_isboolean(L, -1)) return fail("jit.status must return a boolean");
  lua_pop(L, 2);

  /* 5.5-only: no legacy bit library, no deprecated math functions. */
  lua_getglobal(L, "bit");
  if (!lua_isnil(L, -1)) return fail("legacy bit library must not exist");
  lua_pop(L, 1);
  lua_getglobal(L, "math");
  lua_getfield(L, -1, "pow");
  if (!lua_isnil(L, -1)) return fail("deprecated math.pow must not exist");
  lua_pop(L, 2);

  /* 'global' is always a reserved word. */
  if (luaL_loadstring(L, "local global = 1") != LUA_ERRSYNTAX)
    return fail("'global' must be a reserved word");
  lua_pop(L, 1);

  /* run a chunk to prove the VM is live */
  if (luaL_dostring(L, "return 6*7") != LUA_OK) return fail("dostring");
  if (lua_tointeger(L, -1) != 42) return fail("arithmetic");

  lua_close(L);
  printf("embed ok\n");
  return 0;
}
