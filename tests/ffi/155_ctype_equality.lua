-- ffi/155_ctype_equality.lua -- CType objects compare only with CType
-- objects, never with a cdata value of the denoted type.
--
-- LuaJIT treats the type object and a value as different identities:
-- ffi.typeof("int") == ffi.new("int") is false in both operand orders,
-- while two CType objects of the same interned type are equal.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/155_ctype_equality: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")

ffi.cdef[[
typedef struct CTypeEq { int x; } CTypeEq;
]]

local ti = ffi.typeof("int")
local ii = ffi.new("int")
local tp = ffi.typeof("int *")
local ip = ffi.new("int *")
local ts = ffi.typeof("CTypeEq")
local ss = ffi.new("CTypeEq")

H.eq(ti == ii, false, "int ctype == int cdata")
H.eq(ii == ti, false, "int cdata == int ctype")
H.eq(tp == ip, false, "pointer ctype == pointer cdata")
H.eq(ip == tp, false, "pointer cdata == pointer ctype")
H.eq(ts == ss, false, "struct ctype == struct cdata")
H.eq(ss == ts, false, "struct cdata == struct ctype")

H.eq(ti == ffi.typeof("int"), true, "same int ctype")
H.eq(ts == ffi.typeof("CTypeEq"), true, "same struct ctype")
H.eq(rawequal(ti, ffi.typeof("int")), true, "ctype interning")

-- Ordinary cdata equality is unchanged.
H.eq(ii == ffi.new("int"), true, "scalar cdata equality")
H.eq(ts == ffi.new("CTypeEq"), false, "distinct struct values")
local s2 = ffi.new("CTypeEq")
H.eq(ss == ss, true, "self equality")

print("ffi/155_ctype_equality ok")
