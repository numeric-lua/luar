-- ffi/107_pointer_diff.lua -- pointer differences are Lua integers.
--
-- LuaJIT returns a plain Lua number for 'p - q' (an integer count of
-- elements); Luar used to return an int64_t cdata.  The difference is
-- observable through math.type() and avoids a cdata allocation in a hot
-- pointer walk.
local H = dofile("tests/helpers.lua")
local ffi = require "ffi"

local a = ffi.new("int[8]")
local p = ffi.cast("int *", a)
local q = p + 5

local d = q - p
H.eq(math.type(d), "integer")
H.eq(d, 5)
H.eq(math.type(p - q), "integer")
H.eq(p - q, -5)
H.eq((p + 1) - (p + 1), 0)

-- The result participates in ordinary integer arithmetic and comparisons.
H.eq((q - p) * 2 + 1, 11)
H.check((q - p) == 5 and (q - p) < 6)

-- Pointers with different element sizes still count in elements.
local c = ffi.new("char[8]")
local cp = ffi.cast("char *", c)
H.eq((cp + 7) - cp, 7)

print("ffi/107_pointer_diff ok")
