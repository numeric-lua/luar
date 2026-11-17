/*
** luar_ffi.c -- placeholder FFI library for builds without libffi.
**
** Builds with WITH_FFI=0 use this file: the 'ffi' module is not opened
** at all (luar.features.ffi is false), so this code exists only so that
** a C host calling luaopen_ffi() directly gets a clear error instead of
** a link failure.
**
** See Copyright Notice in lua.h and COPYRIGHT.
*/

#define LUA_LIB
#define LUAR_LIB

#include "lprefix.h"

#include <stddef.h>

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"
#include "luar.h"


LUA_API int luar_has_ffi (void) {
  return 0;
}


LUALIB_API int luaopen_ffi (lua_State *L) {
  return luaL_error(L, "LuaR: this build has no FFI support "
                       "(rebuild with WITH_FFI=1 and libffi)");
}
