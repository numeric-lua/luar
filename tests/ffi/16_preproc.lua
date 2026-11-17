-- ffi/16_preproc.lua -- object-like cdef preprocessing.
local ffi = require "ffi"
local H = dofile("tests/helpers.lua")

ffi.cdef[[
#define LUAR_PP_N 4
#define LUAR_PP_UINT unsigned int
struct PpStruct { int a[LUAR_PP_N]; LUAR_PP_UINT b; };
]]
H.eq(ffi.sizeof("struct PpStruct"), 20)

-- definitions persist across ffi.cdef calls
ffi.cdef[[
#define LUAR_PP_FLAG 1
]]
ffi.cdef[[
#if LUAR_PP_FLAG
struct PpOn { int x; };
#else
struct PpOff { double x; };
#endif
]]
H.eq(ffi.sizeof("struct PpOn"), 4)
H.raises(function () return ffi.sizeof("struct PpOff") end, "unknown")

-- skipped branches are not parsed at all
ffi.cdef[[
#if 0
this is not valid C at all
#endif
#ifdef LUAR_PP_MISSING
also not valid
#else
struct PpElse { int y; };
#endif
]]
H.eq(ffi.sizeof("struct PpElse"), 4)

-- #undef and redefinition
ffi.cdef[[
#undef LUAR_PP_N
#define LUAR_PP_N 8
struct PpRedef { int a[LUAR_PP_N]; };
]]
H.eq(ffi.sizeof("struct PpRedef"), 32)

-- macros are not expanded inside strings or comments
ffi.cdef[[
#define LUAR_PP_TEXT 123
/* LUAR_PP_TEXT must stay a comment */
struct PpComment { int v; };
]]
H.eq(ffi.sizeof("struct PpComment"), 4)

-- unsupported forms fail clearly
H.raises(function ()
  return ffi.cdef("#define LUAR_PP_FN(x) (x)")
end, "not supported")
H.raises(function ()
  return ffi.cdef("#include <stdio.h>")
end, "unsupported preprocessor")
H.raises(function ()
  return ffi.cdef("#if 1 + 2\n#endif")
end, "unsupported #if")

print("ffi/16_preproc ok")
