-- ffi/166_preproc_line_continuation.lua -- backslash-newline splicing.
local H = dofile("tests/helpers.lua")

if not pcall(require, "ffi") then
  print("ffi/166_preproc_line_continuation: skipped (FFI unavailable)")
  return
end

local ffi = require("ffi")

ffi.cdef[[
#define PP_LC_SUM(a, b) \
  ((a) + \
   (b))

#define PP_LC_OBJ \
  42

enum {
  PP_LC_SUM_OK = PP_LC_SUM(3, 4),
  PP_LC_OBJ_OK = PP_LC_OBJ,
};

#if defined PP_LC_OBJ && \
    PP_LC_OBJ == 42
typedef int pp_lc_if_ok;
#else
typedef long pp_lc_if_bad;
#endif
]]

H.eq(ffi.C.PP_LC_SUM_OK, 7, "multiline function-like macro")
H.eq(ffi.C.PP_LC_OBJ_OK, 42, "multiline object-like macro")
H.eq(ffi.sizeof("pp_lc_if_ok"), 4, "multiline #if expression")

-- A continuation inside a // comment keeps the next physical line in the
-- comment instead of turning it into a declaration.
ffi.cdef[[
// comment continues \
typedef int pp_lc_comment_must_not_exist;
typedef int pp_lc_after_comment;
]]
H.raises(function () return ffi.sizeof("pp_lc_comment_must_not_exist") end,
         "expected a C type")
H.eq(ffi.sizeof("pp_lc_after_comment"), 4, "declaration after comment")

print("ffi/166_preproc_line_continuation ok")
