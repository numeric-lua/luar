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

-- function-like macros substitute parameters and expand nested calls
ffi.cdef[[
#define LUAR_PP_FIELD(T, N) T b[N]
#define LUAR_PP_ADD(a, b) ((a) + (b))
#define LUAR_PP_WRAP(x) x
struct PpFn { LUAR_PP_FIELD(int, 4); };
enum { LUAR_PP_SUM = LUAR_PP_WRAP(LUAR_PP_ADD(2, 3)) };
]]
H.eq(ffi.sizeof("struct PpFn"), 16)
H.eq(ffi.C.LUAR_PP_SUM, 5)
ffi.cdef[[
#define LUAR_PP_PAREN(a, b) ((a) + (b))
enum { LUAR_PP_PAR = LUAR_PP_PAREN((1 + 2), 4) };
]]
H.eq(ffi.C.LUAR_PP_PAR, 7)

-- variadic macros join the remaining arguments as __VA_ARGS__
ffi.cdef[[
#define LUAR_PP_ARR(...) int b[__VA_ARGS__]
#define LUAR_PP_ADDV(a, ...) ((a) + (__VA_ARGS__))
#define LUAR_PP_ONE(...) 1
struct PpVa { LUAR_PP_ARR(4); };
enum { LUAR_PP_VSUM = LUAR_PP_ADDV(2, 3) };
enum { LUAR_PP_VZERO = LUAR_PP_ONE() };
]]
H.eq(ffi.sizeof("struct PpVa"), 16)
H.eq(ffi.C.LUAR_PP_VSUM, 5)
H.eq(ffi.C.LUAR_PP_VZERO, 1)

-- unsupported forms fail clearly
H.raises(function ()
  return ffi.cdef("#define LUAR_PP_STR(x) #x")
end, "stringification")
H.raises(function ()
  return ffi.cdef("#include <stdio.h>")
end, "unsupported preprocessor")
H.raises(function ()
  return ffi.cdef("#if 1 + 2\n#endif")
end, "unsupported #if")

print("ffi/16_preproc ok")
