/*
** luar_ffi.h -- internal interface to the LuaR FFI library.
**
** The public entry point is luaopen_ffi(); the rest of the file is the
** contract between the cdef parser, the ctype registry, the cdata
** runtime and the call layer.  Everything lives in luar_ffi*.c.
**
** See Copyright Notice in lua.h and COPYRIGHT.
*/

#ifndef luar_ffi_h
#define luar_ffi_h

#include "lua.h"


/* Registry key holding the per-state ffi state. */
#define LUAR_FFI_REGISTRY	"luar.ffi.state"

/* Value used for ffi.NULL (a light userdata NULL pointer). */
#define LUAR_FFI_NULL		((void *)0)


LUALIB_API int luaopen_ffi (lua_State *L);

#endif
