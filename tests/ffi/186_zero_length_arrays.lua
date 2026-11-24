-- ffi/186_zero_length_arrays.lua -- fixed zero-length arrays.
--
-- C headers use `T[0]` as a trailing flexible array.  Luar accepts it as a
-- zero-sized fixed array: sizeof/new are 0, a struct member has zero size
-- and the child alignment, and indexing is bounds-checked (unlike LuaJIT's
-- unchecked out-of-bounds access).
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

H.eq(ffi.sizeof("int[0]"), 0, "zero array size")
local a = ffi.new("int[0]")
H.eq(ffi.sizeof(a), 0, "zero array cdata size")
H.eq(ffi.typeof("int[0]") == ffi.typeof("int[0]"), true, "interned type")
H.raises(function () return a[0] end, "out of bounds")

local b = ffi.new("char[0]")
H.eq(ffi.string(b, 0), "", "zero-length string")

ffi.cdef[[
typedef struct { int n; char data[0]; int tail; } Z0;
typedef struct { Z0 z[0]; int after; } Z1;
]]
H.eq(ffi.sizeof("Z0"), 8, "zero array struct size")
H.eq(ffi.offsetof("Z0", "data"), 4, "zero array offset")
H.eq(ffi.offsetof("Z0", "tail"), 4, "field after zero array")
H.eq(ffi.sizeof("Z1"), 4, "zero array member size")
H.eq(ffi.offsetof("Z1", "after"), 0, "zero array member offset")

local z = ffi.new("Z0")
z.n = 7
z.tail = 9
H.eq(z.n, 7, "struct lead")
H.eq(z.tail, 9, "struct tail")

H.raises(function () ffi.sizeof("int[-1]") end, "negative")

print("ffi/186_zero_length_arrays ok")
