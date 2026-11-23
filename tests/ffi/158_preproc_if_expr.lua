-- ffi/158_preproc_if_expr.lua -- C preprocessor #if expressions.
--
-- Only a single literal, defined(NAME) and !defined(NAME) used to work.
-- Real declarations need boolean operators, comparisons, arithmetic and
-- defined without parentheses.  The evaluator handles the integer C
-- operators, octal/hex/binary literals and character constants.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/158_preproc_if_expr: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")

ffi.cdef [[
#define PP_A 2
#define PP_B 3
#if PP_A + PP_B == 5
enum { PP_SUM = 1 };
#else
#error arithmetic #if failed
#endif

#if defined PP_A && defined(PP_B)
enum { PP_DEFINED = 1 };
#endif

#if !defined(PP_MISSING) && (PP_A * PP_B == 6)
enum { PP_NEGATED = 1 };
#endif

#if (010 == 8) && (0b10 == 2) && (0x10 == 16)
enum { PP_BASES = 1 };
#endif

#if ('A' == 65) && ('\n' == 10)
enum { PP_CHARS = 1 };
#endif

#if 0
this is not parsed at all
#else
enum { PP_ELSE = 1 };
#endif
]]

H.eq(ffi.C.PP_SUM, 1)
H.eq(ffi.C.PP_DEFINED, 1)
H.eq(ffi.C.PP_NEGATED, 1)
H.eq(ffi.C.PP_BASES, 1)
H.eq(ffi.C.PP_CHARS, 1)
H.eq(ffi.C.PP_ELSE, 1)

-- An undefined identifier is zero in C preprocessor expressions.
ffi.cdef [[
#if PP_UNDEFINED_IDENTIFIER == 0
enum { PP_UNKNOWN_ZERO = 1 };
#endif
]]
H.eq(ffi.C.PP_UNKNOWN_ZERO, 1)

H.raises(function () ffi.cdef("#if 1 +\n#endif\n") end,
         "unsupported #if expression")
H.raises(function () ffi.cdef("#if 1 / 0\n#endif\n") end,
         "division by zero in #if")
H.raises(function () ffi.cdef("#if defined()\n#endif\n") end,
         "expected a macro name")

print("ffi/158_preproc_if_expr ok")
