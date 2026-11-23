-- ffi/123_new_pointer_strict.lua -- ffi.new pointer initializers follow
-- LuaJIT's type rules; ffi.cast keeps C's permissive conversions.
--
-- ffi.new("int *", 12) used to produce address 12 and ffi.new("int *",
-- char_array) silently reinterpreted incompatible storage.  ffi.new now
-- accepts only nil, a char-like Lua string (the existing Luar convenience),
-- a light userdata, a compatible pointer/array, or an aggregate of the
-- exact pointee type; numbers, numeric cdata and incompatible pointers are
-- rejected.  Casts remain permissive because that is what a C cast is.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/123_new_pointer_strict: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")

ffi.cdef[[
typedef struct PtrS { int x; } PtrS;
typedef struct PtrT { int x; } PtrT;
]]

local ip = ffi.new("int[2]")
local cp = ffi.new("char[4]")
local vp = ffi.new("void *", nil)
local s = ffi.new("PtrS")
local t = ffi.new("PtrT")

-- Numbers and numeric cdata are not addresses for ffi.new.
H.raises(function() ffi.new("int *", 12) end, "cannot convert")
H.raises(function() ffi.new("int *", 0) end, "cannot convert")
H.raises(function() ffi.new("int *", ffi.new("int", 3)) end, "cannot convert")
H.raises(function() ffi.new("int *", true) end, "cannot convert")
H.raises(function() ffi.new("int *", "abc") end, "cannot convert")

-- nil still means NULL, and array/pointer of the same base type decays.
H.eq(tostring(ffi.new("int *", nil)), "cdata<int *>: NULL")
H.eq(tostring(ffi.new("int *")), "cdata<int *>: NULL")
H.check(ffi.new("int *", ip) ~= nil, "int[] should initialise int *")
H.check(ffi.new("int *", ip + 0) ~= nil, "int * should initialise int *")
H.check(ffi.new("void *", ip) ~= nil, "int[] should initialise void *")
H.eq(tostring(ffi.new("int *", vp)), "cdata<int *>: NULL")

-- Incompatible pointers and aggregate tags are rejected instead of being
-- reinterpreted at the same address.
H.raises(function() ffi.new("int *", cp) end, "cannot convert")
H.raises(function() ffi.new("char *", ip) end, "cannot convert")
H.raises(function() ffi.new("PtrS *", ip) end, "cannot convert")
H.raises(function() ffi.new("PtrS *", t) end, "cannot convert")
H.check(ffi.new("PtrS *", s) ~= nil, "same struct type should initialise pointer")

-- The char-like string convenience predates this check and stays.
H.eq(ffi.string(ffi.new("char *", "abc")), "abc")
H.eq(ffi.string(ffi.new("const char *", "abc")), "abc")

-- ffi.cast is not ffi.new: C casts may still form any address.
H.eq(tostring(ffi.cast("intptr_t", ffi.cast("int *", 12))), "12LL")
H.check(ffi.cast("int *", cp) ~= nil, "cast should accept incompatible pointer")

print("ffi/123_new_pointer_strict ok")
