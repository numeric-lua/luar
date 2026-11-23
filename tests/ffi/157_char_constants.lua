-- ffi/157_char_constants.lua -- C character constants in ffi.cdef.
--
-- Character constants are ordinary integer constant expressions in C, so
-- they are valid enum values and array sizes.  LuaJIT's cdef accepts a
-- single plain or escaped character and sign-extends through plain char;
-- multi-character constants are rejected instead of guessed.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/157_char_constants: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")

ffi.cdef [[
enum {
  CHR_A = 'A',
  CHR_NL = '\n',
  CHR_TAB = '\t',
  CHR_NUL = '\0',
  CHR_HEX = '\x41',
  CHR_OCT = '\101',
  CHR_QUOTE = '\'',
  CHR_SLASH = '\\',
  CHR_FF = '\xff',
  CHR_QUESTION = '?'
};
typedef struct { char a['A']; char b[CHR_OCT]; } CharSized;
]]

H.eq(ffi.C.CHR_A, 65)
H.eq(ffi.C.CHR_NL, 10)
H.eq(ffi.C.CHR_TAB, 9)
H.eq(ffi.C.CHR_NUL, 0)
H.eq(ffi.C.CHR_HEX, 65)
H.eq(ffi.C.CHR_OCT, 65)
H.eq(ffi.C.CHR_QUOTE, 39)
H.eq(ffi.C.CHR_SLASH, 92)
H.eq(ffi.C.CHR_FF, -1)       -- plain char is signed on the tested targets
H.eq(ffi.C.CHR_QUESTION, 63)
H.eq(ffi.sizeof("CharSized"), 130)

H.raises(function () ffi.cdef("enum { CHR_BAD_MULTI = 'AB' };") end,
         "multi%-character")
H.raises(function () ffi.cdef("enum { CHR_BAD_EMPTY = '' };") end,
         "empty character constant")
H.raises(function () ffi.cdef("enum { CHR_BAD_ESC = '\\q' };") end,
         "unsupported escape")
H.raises(function () ffi.cdef("enum { CHR_BAD_END = 'x };") end,
         "unterminated character constant")

print("ffi/157_char_constants ok")
