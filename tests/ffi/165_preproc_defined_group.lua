-- ffi/165_preproc_defined_group.lua -- unparenthesised 'defined' must not
-- eat the ')' that closes an enclosing #if group.
local H = dofile("tests/helpers.lua")

if not pcall(require, "ffi") then
  print("ffi/165_preproc_defined_group: skipped (FFI unavailable)")
  return
end

local ffi = require("ffi")

ffi.cdef[[
#define PP_DEF_A 1
#define PP_DEF_B 1

#if (defined PP_DEF_A || defined PP_DEF_B)
enum { PP_DEF_AB = 11 };
#endif

#if (defined PP_DEF_A && defined PP_DEF_MISSING)
enum { PP_DEF_BAD = 12 };
#endif

#if defined PP_DEF_A || defined PP_DEF_MISSING
enum { PP_DEF_ONE = 13 };
#endif

#if (defined(PP_DEF_A) && defined ( PP_DEF_B ))
enum { PP_DEF_PARENS = 14 };
#endif

#undef PP_DEF_B
#if (defined PP_DEF_A || defined PP_DEF_B)
enum { PP_DEF_AFTER_UNDEF = 15 };
#endif
]]

H.eq(ffi.C.PP_DEF_AB, 11, "defined in a parenthesised group")
H.check(not pcall(function () return ffi.C.PP_DEF_BAD end),
        "&& with an undefined operand must be false")
H.eq(ffi.C.PP_DEF_ONE, 13, "bare defined operands")
H.eq(ffi.C.PP_DEF_PARENS, 14, "both defined() spellings")
H.eq(ffi.C.PP_DEF_AFTER_UNDEF, 15, "defined after #undef")

-- The parenthesised form still needs its own closing parenthesis.
H.raises(function ()
  ffi.cdef("#if defined(PP_DEF_A\n#endif\n")
end, "expected '%)'")

-- A bare 'defined' still needs a macro name.
H.raises(function ()
  ffi.cdef("#if defined()\n#endif\n")
end, "expected a macro name")

print("ffi/165_preproc_defined_group ok")
