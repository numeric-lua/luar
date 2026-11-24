-- ffi/167_preproc_ternary.lua -- C's conditional operator in #if.
local H = dofile("tests/helpers.lua")

if not pcall(require, "ffi") then
  print("ffi/167_preproc_ternary: skipped (FFI unavailable)")
  return
end

local ffi = require("ffi")

ffi.cdef[[
#define PP_CT 1
#define PP_CF 0

#if PP_CT ? 1 : 0
enum { PP_TERN_TRUE = 1 };
#endif

#if PP_CF ? 1 : 0
enum { PP_TERN_FALSE = 2 };
#endif

#if (PP_CF ? 0 : 0 ? 2 : 3) == 3
enum { PP_TERN_RIGHT = 3 };
#endif

#if (PP_CT || PP_CF ? 1 : 0)
enum { PP_TERN_PREC = 4 };
#endif

#if (PP_CT ? (PP_CT ? 5 : 6) : 7) == 5
enum { PP_TERN_NESTED = 5 };
#endif

#if defined PP_CT ? PP_CT : 0
enum { PP_TERN_DEFINED = 6 };
#endif

/* Only the selected branch is evaluated. */
#if PP_CT ? 1 : 1 / 0
enum { PP_TERN_LAZY_TRUE = 7 };
#endif
#if PP_CF ? 1 / 0 : 1
enum { PP_TERN_LAZY_FALSE = 8 };
#endif
#if PP_CT || 1 / 0
enum { PP_TERN_OR = 9 };
#endif
]]

H.eq(ffi.C.PP_TERN_TRUE, 1)
H.check(not pcall(function () return ffi.C.PP_TERN_FALSE end),
        "false ternary branch defined an enum")
H.eq(ffi.C.PP_TERN_RIGHT, 3)
H.eq(ffi.C.PP_TERN_PREC, 4)
H.eq(ffi.C.PP_TERN_NESTED, 5)
H.eq(ffi.C.PP_TERN_DEFINED, 6)
H.eq(ffi.C.PP_TERN_LAZY_TRUE, 7)
H.eq(ffi.C.PP_TERN_LAZY_FALSE, 8)
H.eq(ffi.C.PP_TERN_OR, 9)

-- A missing ':' is a syntax error.
H.raises(function ()
  ffi.cdef("#if 1 ? 2\n#endif\n")
end, "expected ':'")

-- A live division by zero is still rejected, including in an #elif.
H.raises(function ()
  ffi.cdef("#if 1 / 0\n#endif\n")
end, "division by zero")
H.raises(function ()
  ffi.cdef("#if 0\n#elif 1 ? 1 / 0 : 0\n#endif\n")
end, "division by zero")

print("ffi/167_preproc_ternary ok")
