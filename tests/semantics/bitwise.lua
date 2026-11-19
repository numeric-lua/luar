-- semantics/bitwise.lua -- native Lua 5.5 bitwise operators.
--
-- This file must pass unchanged under stock Lua 5.5.1 and Luar.  There is
-- no `bit` library: these are language operators.

local H = dofile("tests/helpers.lua")

-- elementary operations
H.eq(7 & 3, 3)
H.eq(7 | 8, 15)
H.eq(7 ~ 3, 4)
H.eq(~0, -1)
H.eq(~1, -2)
H.eq(5 >> 1, 2)
H.eq(1 << 62, 4611686018427387904)
H.eq(8.0 & 1, 0)                 -- exact float is convertible
H.eq(0xffffffffffffffff, -1)     -- hex literals are 64-bit integers
H.eq(math.type(7 & 3), "integer")
H.eq(math.type(1 << 1), "integer")

-- precedence: | < ~ < & < shift < + - ...
H.eq(1 | 2 & 3, 3)               -- 1 | (2 & 3)
H.eq(1 + 2 << 3, 24)             -- (1 + 2) << 3
H.eq(2 ^ 3 & 1, 0)               -- (2.0 ^ 3) & 1

-- 64-bit wrap and logical (not arithmetic) right shift
H.eq(1 << 63, math.mininteger)
H.eq((1 << 63) >> 63, 1)
H.eq(math.mininteger >> 1, 4611686018427387904)
H.eq(-1 >> 1, math.maxinteger)
H.eq(math.maxinteger & -1, math.maxinteger)

-- displacements: negative reverses direction, |n| >= 64 yields zero
H.eq(1 << -1, 0)
H.eq(1 >> -1, 2)
H.eq(1 << 64, 0)
H.eq(1 << -64, 0)
H.eq(1 >> 64, 0)

-- operands must be integers or exactly convertible floats
H.raises(function () return 1.5 & 1 end, "no integer representation")
H.raises(function () return 2.5 | 1 end, "no integer representation")
H.eq(2.0 & 1, 0)
H.raises(function () return "3" & 1 end, "bitwise operation on a string")

-- metamethods for non-number operands
local mt = {
  __band = function () return 40 end,
  __bor = function () return 41 end,
  __bxor = function () return 42 end,
  __shl = function () return 43 end,
  __shr = function () return 44 end,
  __bnot = function () return 45 end,
}
local o = setmetatable({}, mt)
H.eq(o & 1, 40)
H.eq(1 & o, 40)
H.eq(o | 1, 41)
H.eq(o ~ 1, 42)
H.eq(o << 1, 43)
H.eq(o >> 1, 44)
H.eq(~o, 45)

-- bitwise operators do not coerce strings (unlike arithmetic)
H.eq("10" + 1, 11)
H.raises(function () return "10" | 1 end, "bitwise operation on a string")

print("semantics/bitwise ok")
