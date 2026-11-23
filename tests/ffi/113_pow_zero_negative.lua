-- ffi/113_pow_zero_negative.lua -- cdata integer power with a zero base
-- and a negative exponent.
--
-- C/LuaJIT define x^-n through the floating result.  0^-1 is +infinity,
-- which LuaJIT's int64 conversion turns into INT64_MAX.  The integer power
-- helper treated every negative exponent except +/-1 as zero, so the
-- sentinel was lost and `0 ^ -1` returned 0.
local H = dofile("tests/helpers.lua")

local ffi = require "ffi"

local function pow (base, exp)
  return ffi.new("int64_t", base) ^ exp
end

H.eq(tonumber(pow(0, -1)), math.maxinteger, "0 ^ -1")
H.eq(tonumber(pow(0, -2)), math.maxinteger, "0 ^ -2")
H.eq(tonumber(pow(0, math.mininteger)), math.maxinteger,
     "0 ^ math.mininteger")
H.eq(tonumber(ffi.new("int64_t", 0.0) ^ -1), math.maxinteger,
     "0.0 cdata ^ -1")

-- The zero-power and positive-power cases are unchanged.
H.eq(tonumber(pow(0, 0)), 1, "0 ^ 0")
H.eq(tonumber(pow(0, 1)), 0, "0 ^ 1")

-- A negative base keeps its existing negative-exponent rules.
H.eq(tonumber(pow(1, -5)), 1, "1 ^ -5")
H.eq(tonumber(pow(-1, -5)), -1, "-1 ^ -5")
H.eq(tonumber(pow(-1, -4)), 1, "-1 ^ -4")
H.eq(tonumber(pow(2, -1)), 0, "2 ^ -1")
H.eq(tonumber(pow(-2, -1)), 0, "-2 ^ -1")

-- Unsigned arithmetic is unaffected: the exponent is a huge positive
-- value and 0 to any positive power is 0.
local u = ffi.new("uint64_t", 0) ^ -1
H.eq(tonumber(u), 0, "uint64_t 0 ^ -1")
H.eq(ffi.typeof(u), ffi.typeof("uint64_t"), "unsigned result type")

-- Results are int64 cdata, not Lua numbers.
H.eq(ffi.typeof(pow(0, -1)), ffi.typeof("int64_t"), "int64 result type")

print("ffi/113_pow_zero_negative ok")
