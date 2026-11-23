-- ffi/126_void_pointer_aggregate.lua -- aggregate lvalues convert to void *
-- in ffi.new and pointer assignments, as in C and LuaJIT.
--
-- The strict ffi.new pointer rules accepted only an aggregate whose exact
-- type matched the pointee, so `ffi.new("void *", struct_value)` raised even
-- though LuaJIT (and C) accept it.  Numeric scalars and mismatched object
-- pointers remain rejected; this is not a general number-to-pointer cast.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/126_void_pointer_aggregate: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")

ffi.cdef[[
typedef struct { int x; } VoidAggS;
typedef union { int x; } VoidAggU;
typedef struct { void *p; int *ip; } VoidHolder;
]]

local s = ffi.new("VoidAggS")
s.x = 5
local u = ffi.new("VoidAggU")
u.x = 6

local ps = ffi.new("void *", s)
H.check(ps == ffi.cast("void *", s), "struct did not convert to void *")
H.eq(ffi.cast("VoidAggS *", ps).x, 5, "void * did not point at the struct")
local pu = ffi.new("void *", u)
H.check(pu == ffi.cast("void *", u), "union did not convert to void *")
H.eq(ffi.cast("VoidAggU *", pu).x, 6, "void * did not point at the union")

-- The same conversion applies to pointer fields.
local h = ffi.new("VoidHolder")
h.p = s
H.check(h.p == ps, "struct assignment to a void * field")
h.p = u
H.check(h.p == pu, "union assignment to a void * field")

-- Exact-type object pointers keep working.
local exact = ffi.new("VoidAggS *", s)
H.eq(exact.x, 5, "exact struct pointer initializer regressed")
H.eq(ffi.new("VoidAggS *", s).x, 5, "ffi.new exact struct pointer")

-- Arrays still decay, and their pointer is accepted for void *.
local a = ffi.new("int[3]", { 7, 8, 9 })
local pa = ffi.new("void *", a)
H.check(pa == ffi.cast("void *", a), "array to void *")
H.eq(ffi.cast("int *", pa)[2], 9, "void * array address")

-- A mismatched object pointer is still rejected, and numeric scalars are not
-- silently reinterpreted as addresses.
H.raises(function() return ffi.new("int *", s) end, "cannot convert")
H.raises(function() return ffi.new("VoidAggU *", s) end, "cannot convert")
H.raises(function() return ffi.new("void *", ffi.new("int", 5)) end,
         "cannot convert")
H.raises(function() return ffi.new("void *", false) end, "cannot convert")
H.raises(function() h.ip = s end, "cannot convert")
print("ffi/126_void_pointer_aggregate ok")
