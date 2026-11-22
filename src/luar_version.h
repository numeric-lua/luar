/*
** luar_version.h -- Luar version macros.
**
** Luar is a Lua 5.5-forward runtime: the language and C API track the
** official PUC-Rio Lua releases, while the runtime adds LuaJIT-style
** extensions (jit, ffi, table.new/clear) and a tiered JIT.
**
** See Copyright Notice in lua.h and COPYRIGHT.
*/

#ifndef luar_version_h
#define luar_version_h

#define LUAR_VERSION_MAJOR	0
#define LUAR_VERSION_MINOR	127
#define LUAR_VERSION_PATCH	0

#define LUAR_VERSION		"0.127.0"
#define LUAR_VERSION_NUM	(LUAR_VERSION_MAJOR * 10000 + \
				 LUAR_VERSION_MINOR * 100 + \
				 LUAR_VERSION_PATCH)

/* Version of the Lua language/runtime this build is based on. */
#define LUAR_LUA_VERSION	"Lua 5.5.1"
#define LUAR_LUA_VERSION_NUM	505

#define LUAR_COPYRIGHT		"Luar " LUAR_VERSION \
				"  Copyright (C) 2026 Luar contributors"

/* One-line identity banner used by the command line tools. */
#define LUAR_BANNER		"Luar " LUAR_VERSION " (" LUAR_LUA_VERSION ")" \
				"  Copyright (C) 2026 Luar contributors"

#endif
