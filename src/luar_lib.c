/*
** luar_lib.c -- LuaR runtime library: identity table, LuaJIT-style
** extensions to the standard libraries, and the extended openlibs().
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


/* ---------------------------------------------------------------- */
/* table.new / table.clear (LuaJIT extensions)                      */
/* ---------------------------------------------------------------- */

static int table_new (lua_State *L) {
  lua_Integer narr = luaL_optinteger(L, 1, 0);
  lua_Integer nrec = luaL_optinteger(L, 2, 0);
  lua_createtable(L, (int)narr, (int)nrec);
  return 1;
}


static int table_clear (lua_State *L) {
  luaL_checktype(L, 1, LUA_TTABLE);
  lua_pushnil(L);
  /* Setting an existing field to nil while traversing is explicitly
  ** allowed; the key stays valid for the next lua_next().
  */
  while (lua_next(L, 1) != 0) {
    lua_pop(L, 1);              /* remove value, keep key */
    lua_pushvalue(L, -1);       /* duplicate key */
    lua_pushnil(L);
    lua_settable(L, 1);         /* t[key] = nil */
  }
  return 0;
}


static void add_table_extras (lua_State *L) {
  lua_getglobal(L, LUA_TABLIBNAME);
  if (lua_istable(L, -1)) {
    lua_pushcfunction(L, table_new);
    lua_setfield(L, -2, "new");
    lua_pushcfunction(L, table_clear);
    lua_setfield(L, -2, "clear");
  }
  lua_pop(L, 1);
}


/* ---------------------------------------------------------------- */
/* Architecture / OS names (LuaJIT-compatible spelling)             */
/* ---------------------------------------------------------------- */

LUA_API const char *luar_os (void) {
#if defined(_WIN32)
  return "Windows";
#elif defined(__APPLE__)
  return "OSX";
#elif defined(__linux__)
  return "Linux";
#elif defined(__FreeBSD__)
  return "FreeBSD";
#elif defined(__unix__)
  return "POSIX";
#else
  return "Other";
#endif
}


LUA_API const char *luar_arch (void) {
#if defined(__x86_64__) || defined(_M_X64)
  return "x64";
#elif defined(__i386__) || defined(_M_IX86)
  return "x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
  return "arm64";
#elif defined(__arm__) || defined(_M_ARM)
  return "arm";
#elif defined(__powerpc64__)
  return "ppc64";
#elif defined(__powerpc__)
  return "ppc";
#elif defined(__riscv) && (__riscv_xlen == 64)
  return "riscv64";
#else
  return "unknown";
#endif
}


/* ---------------------------------------------------------------- */
/* luar table                                                       */
/* ---------------------------------------------------------------- */

static const luaL_Reg luarlib[] = {
  {NULL, NULL}
};


LUALIB_API int luaopen_luar (lua_State *L) {
  luaL_checkversion(L);
  luaL_newlib(L, luarlib);
  lua_pushliteral(L, LUAR_VERSION);
  lua_setfield(L, -2, "version");
  lua_pushinteger(L, LUAR_VERSION_NUM);
  lua_setfield(L, -2, "version_num");
  lua_pushliteral(L, LUAR_LUA_VERSION);
  lua_setfield(L, -2, "lua_version");
  lua_pushliteral(L, LUAR_COPYRIGHT);
  lua_setfield(L, -2, "copyright");
  lua_pushstring(L, luar_os());
  lua_setfield(L, -2, "os");
  lua_pushstring(L, luar_arch());
  lua_setfield(L, -2, "arch");

  /* Feature matrix, so portable code can probe without pcall. */
  lua_newtable(L);
  lua_pushboolean(L, luar_has_ffi());
  lua_setfield(L, -2, "ffi");
  lua_pushboolean(L, 1);
  lua_setfield(L, -2, "table");
  lua_pushboolean(L, luar_has_jit());
  lua_setfield(L, -2, "jit");
  lua_setfield(L, -2, "features");
  return 1;
}


/* ---------------------------------------------------------------- */
/* Extended openlibs                                               */
/* ---------------------------------------------------------------- */

/*
** Register a library in package.preload as well as loading it, so that
** both 'require("jit")' and the global work (LuaJIT behaviour).
*/
static void open_extension (lua_State *L, const char *name,
                            lua_CFunction opener) {
  luaL_getsubtable(L, LUA_REGISTRYINDEX, LUA_PRELOAD_TABLE);
  lua_pushcfunction(L, opener);
  lua_setfield(L, -2, name);
  lua_pop(L, 1);
  luaL_requiref(L, name, opener, 1);
  lua_pop(L, 1);
}


LUALIB_API void luar_openlibs (lua_State *L) {
  luaL_openselectedlibs(L, ~0, 0);   /* all standard libraries */
  open_extension(L, LUAR_JITLIBNAME, luaopen_jit);
  if (luar_has_ffi())
    open_extension(L, LUAR_FFILIBNAME, luaopen_ffi);
  open_extension(L, LUAR_LIBNAME, luaopen_luar);
  add_table_extras(L);
}


LUA_API const char *luar_version (void) {
  return LUAR_VERSION;
}


LUA_API const char *luar_lua_version (void) {
  return LUAR_LUA_VERSION;
}
