-- ffi/162_array_init_aggregate.lua -- an aggregate or pointer cdata is not
-- a scalar element initializer for ffi.new(array).
--
-- `ffi.new("int[3]", other_array)` used to fall through to the scalar
-- initializer path: ffi_to_int() converted the source array's address to an
-- integer, and init_array_args() broadcast that truncated pointer over the
-- whole destination.  For a shorter source array that even copied the
-- declared element count out of bounds.  A mismatched aggregate must be a
-- clear error, while scalar cdata (int/double/bool) may still broadcast.
local ffi = require "ffi"
local H = dofile("tests/helpers.lua")

local a2 = ffi.new("int[2]", {1, 2})
local a3 = ffi.new("int[3]", {1, 2, 3})
local p = ffi.cast("int *", a3)

H.raises(function () ffi.new("int[3]", a2) end,
         "incompatible array initializer")
H.raises(function () ffi.new("int[2]", a3) end,
         "incompatible array initializer")
H.raises(function () ffi.new("int[?]", 3, a3) end,
         "incompatible array initializer")
H.raises(function () ffi.new("int[?]", 3, a2) end,
         "incompatible array initializer")
H.raises(function () ffi.new("int[3]", p) end,
         "incompatible array initializer")
H.raises(function () ffi.new("struct { int x; }[2]", a3) end,
         "incompatible array initializer")

-- The exact same array type still copies by value.
local copy = ffi.new("int[3]", a3)
H.eq(copy[0], 1)
H.eq(copy[1], 2)
H.eq(copy[2], 3)
copy[0] = 9
H.eq(a3[0], 1, "the copy must not alias its source")

-- Scalar cdata keeps its documented broadcast behaviour.
local c = ffi.new("int", 7)
local d = ffi.new("double", 1.5)
local x = ffi.new("int[3]", c)
H.eq(x[0], 7)
H.eq(x[1], 7)
H.eq(x[2], 7)
local y = ffi.new("int[3]", d)
H.eq(y[0], 1)
H.eq(y[1], 1)
H.eq(y[2], 1)

-- A table initializer and a string initializer are unaffected.
local z = ffi.new("int[3]", {4, 5})
H.eq(z[0], 4)
H.eq(z[1], 5)
H.eq(z[2], 0)
local s = ffi.new("char[4]", "abc")
H.eq(ffi.string(s), "abc")

print("ffi/162_array_init_aggregate ok")
