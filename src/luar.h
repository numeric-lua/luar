/*
** luar.h -- public extensions provided by the LuaR runtime.
**
** LuaR keeps the complete Lua 5.5 C API and ABI (lua.h, lauxlib.h,
** lualib.h).  This header only declares the additions.  Include lua.h
** first (or just include this header, which does it for you).
**
** See Copyright Notice in lua.h and COPYRIGHT.
*/

#ifndef luar_h
#define luar_h

#include "lua.h"
#include "luar_version.h"

/* Names of the extension libraries. */
#define LUAR_LIBNAME		"luar"
#define LUAR_JITLIBNAME		"jit"
#define LUAR_FFILIBNAME		"ffi"

/*
** Open the standard libraries plus the LuaR extensions.  This is what
** the luaL_openlibs() macro expands to in LuaR.
*/
LUALIB_API void luar_openlibs (lua_State *L);

/* Library openers, also usable through require(). */
LUALIB_API int luaopen_luar (lua_State *L);
LUALIB_API int luaopen_jit (lua_State *L);
LUALIB_API int luaopen_ffi (lua_State *L);

/* Runtime identity. */
LUA_API const char *luar_version (void);
LUA_API const char *luar_lua_version (void);
LUA_API const char *luar_os (void);
LUA_API const char *luar_arch (void);

/*
** Build feature test: nonzero when the FFI library is compiled in.
** (Declared as a function so it is usable from any language binding.)
*/
LUA_API int luar_has_ffi (void);

#endif
