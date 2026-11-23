-- ffi/156_int_literal_bases.lua -- C integer literal bases in cdef constants.
--
-- The declaration lexer used decimal scanning for every unsuffixed literal,
-- so 010 was ten instead of eight and 0b1010 was not accepted at all.  C
-- (and LuaJIT's cdef) use a leading zero for octal and accept the C23/GCC
-- 0b prefix.  Array sizes and enum values share the same parser.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/156_int_literal_bases: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")

ffi.cdef [[
enum {
  LIT_DEC = 10,
  LIT_OCT = 010,
  LIT_OCT_BIG = 0777,
  LIT_HEX = 0x10,
  LIT_BIN = 0b1010,
  LIT_BIN_UP = 0B1000,
  LIT_SUFFIX = 0x20ULL,
  LIT_ZERO = 0
};
typedef struct {
  char a[LIT_OCT];
  char b[LIT_BIN];
} LitArrayBases;
]]

H.eq(ffi.C.LIT_DEC, 10)
H.eq(ffi.C.LIT_OCT, 8)
H.eq(ffi.C.LIT_OCT_BIG, 511)
H.eq(ffi.C.LIT_HEX, 16)
H.eq(ffi.C.LIT_BIN, 10)
H.eq(ffi.C.LIT_BIN_UP, 8)
H.eq(ffi.C.LIT_SUFFIX, 32)
H.eq(ffi.C.LIT_ZERO, 0)

H.eq(ffi.sizeof("LitArrayBases"), 18)
H.eq(ffi.offsetof("LitArrayBases", "b"), 8)

-- Invalid digits are rejected rather than silently parsed as a prefix.
H.raises(function () ffi.cdef("enum { LIT_BAD_OCT = 09 };") end,
         "malformed octal constant")
H.raises(function () ffi.cdef("enum { LIT_BAD_BIN = 0b102 };") end,
         "malformed integer constant")
H.raises(function () ffi.cdef("enum { LIT_BAD_HEX = 0x };") end,
         "malformed hexadecimal constant")

print("ffi/156_int_literal_bases ok")
