-- compat/bit_semantics.lua -- values generated with LuaJIT 2.1 and frozen
-- here, so the suite does not depend on LuaJIT being installed.
local H = dofile("tests/helpers.lua")

-- number folding: round to nearest even, then wrap modulo 2^32
local fold = {
  {0, 0}, {1, 1}, {-1, -1}, {0.5, 0}, {1.5, 2}, {2.5, 2}, {-1.5, -2},
  {-2.5, -2}, {3.7, 4}, {-3.7, -4}, {4294967296, 0}, {-4294967296, 0},
  {4294967297, 1}, {0x7fffffff, 0x7fffffff}, {0x80000000, -0x80000000},
  {0xdeadbeef, -0x21524111},   -- 0xdeadbeef as signed int32
}
for _, c in ipairs(fold) do
  H.eq(bit.tobit(c[1]), c[2], "tobit(" .. tostring(c[1]) .. ")")
end

-- shifts use five bits of the count; negative counts wrap
H.eq(bit.lshift(1, 0), 1)
H.eq(bit.lshift(1, 31), -0x80000000)
H.eq(bit.lshift(1, 32), 1)
H.eq(bit.lshift(1, -1), -0x80000000)
H.eq(bit.rshift(-1, 31), 1)
H.eq(bit.arshift(-2, 1), -1)
H.eq(bit.arshift(0x40000000, 30), 1)
H.eq(bit.rol(0x12345678, 8), 0x34567812)
H.eq(bit.rol(0x12345678, 0), 0x12345678)
H.eq(bit.ror(0x12345678, 8), 0x78123456)
H.eq(bit.bswap(0x12345678), 0x78563412)
H.eq(bit.bswap(0xffffffff), -1)

-- variadic logic
H.eq(bit.band(0xff, 0x0f, 0x03), 3)
H.eq(bit.bor(1, 2, 4), 7)
H.eq(bit.bxor(0xff, 0x0f, 0x03), 0xf3)
H.eq(bit.band(0xffffffff), -1)
H.raises(function() bit.bor() end, "number expected")

-- tohex widths and case, low digits first
H.eq(bit.tohex(0xdeadbeef), "deadbeef")
H.eq(bit.tohex(0xdeadbeef, 4), "beef")
H.eq(bit.tohex(0xdeadbeef, -4), "BEEF")
H.eq(bit.tohex(0xdeadbeef, 1), "f")
H.eq(bit.tohex(0), "00000000")
H.eq(bit.tohex(-1, 2), "ff")

-- string coercion matches Lua's arithmetic coercion
H.eq(bit.band("255", 15), 15)

print("compat/bit_semantics ok")
