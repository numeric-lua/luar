-- ffi/09_format.lua -- string.format with cdata arguments (LuaJIT rules).
local ffi = require "ffi"
local H = dofile("tests/helpers.lua")

local i = ffi.new("int64_t", 42)
local u = ffi.new("uint64_t", 42)
local d = ffi.new("double", 1.5)

H.eq(string.format("%d", i), "42")
H.eq(string.format("%i", u), "42")
H.eq(string.format("%u", i), "42")
H.eq(string.format("%o", i), "52")
H.eq(string.format("%x", u), "2a")
H.eq(string.format("%X", i), "2A")
H.eq(string.format("%05d", ffi.new("int", 7)), "00007")
H.eq(string.format("[%4d|%-4d]", i, u), "[  42|42  ]")

-- %s and %q use cdata tostring, as in LuaJIT
H.eq(string.format("%s", i), "42LL")
H.eq(string.format("%q", i), '"42LL"')
H.eq(string.format("%s %q", i, u), '42LL "42ULL"')

-- format strings without cdata keep stock Lua behavior
H.eq(string.format("%d%%", i), "42%")
H.eq(string.format("plain"), "plain")
H.eq(string.format("%d-%s", 5, "x"), "5-x")

-- float cdata is not accepted by integer/float conversions (LuaJIT rule)
H.raises(function () return string.format("%d", d) end, "number expected")
H.raises(function () return string.format("%f", d) end, "number expected")

-- stock Lua numeric rules still apply to plain Lua numbers
H.raises(function () return string.format("%d", 1.5) end,
         "number has no integer representation")

print("ffi/09_format ok")
