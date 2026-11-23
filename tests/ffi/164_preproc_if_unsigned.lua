-- ffi/164_preproc_if_unsigned.lua -- #if arithmetic follows C's
-- intmax_t/uintmax_t rules.
--
-- The evaluator used a bare int64_t.  Hex/octal/binary literals such as
-- 0xffffffffffffffff were therefore seen as -1, unsigned comparison and
-- division were signed, and an unsigned right shift was arithmetic.  C
-- chooses the literal's type from its value and suffix, and a mixed
-- expression is unsigned; #if must do the same.
local ffi = require "ffi"
local H = dofile("tests/helpers.lua")

local n = 0
local function check (name, expr, want)
  n = n + 1
  local def = "pp_unsigned_" .. n
  local src = string.format(
    "#if %s\ntypedef int %s;\n#else\ntypedef int %s_false;\n#endif",
    expr, def, def)
  ffi.cdef(src)
  local ok = pcall(ffi.typeof, def)
  H.eq(ok, want, name .. ": " .. expr)
end

-- A hex literal above INT64_MAX is unsigned, so it compares as 2^64 - 1.
check("u64 max positive", "0xffffffffffffffff > 0", true)
check("u64 max not negative", "0xffffffffffffffff < 0", false)
check("top bit unsigned", "0x8000000000000000 > 0", true)
check("top bit not negative", "0x8000000000000000 < 0", false)
check("decimal overflow unsigned", "18446744073709551615 > 0", true)

-- Mixed signed/unsigned operands use unsigned arithmetic.
check("mixed compare", "1u < -1", true)
check("mixed equality", "(0u - 1) == 0xffffffffffffffff", true)
check("mixed division", "(-1 / 2u) == 0x7fffffffffffffff", true)
check("mixed modulo", "(0xffffffffffffffff % 3) == 0", true)
check("mixed multiply", "0x7fffffff * 2u > 0", true)

-- Bitwise not and unary minus preserve the (unsigned) operand type.
check("unsigned not", "~0u == 0xffffffffffffffff", true)
check("unsigned negate", "-0x8000000000000000 > 0", true)

-- Right shift is logical for an unsigned left operand.
check("unsigned shift", "(0xffffffffffffffff >> 63) == 1", true)
check("unsigned shift high", "(0x8000000000000000 >> 63) == 1", true)

-- Signed literals and ordinary arithmetic are unchanged.
check("signed compare", "-1 < 1", true)
check("decimal signed", "9223372036854775807 > 0", true)
check("signed shift", "(-8 >> 2) == -2", true)
check("char constant", "'A' == 65", true)
check("octal unsigned", "01777777777777777777777 > 0", true)

-- Suffixes choose the same type tiers as C on this host.
check("u suffix", "0xffffffffu > 0", true)
check("l suffix", "0xffffffffL > 0", true)
if ffi.abi("64bit") then
  -- On LP64 a hex value above INT64_MAX doesn't fit long, so the L suffix
  -- selects unsigned long; without a suffix the first candidate is already
  -- unsigned long long.
  check("ll overflow unsigned", "0x8000000000000000LL > 0", true)
end

-- Invalid shift counts remain clear errors rather than undefined output.
H.raises(function () ffi.cdef("#if (1 << 64) == 0\n#endif\n") end,
         "shift count out of range")
H.raises(function () ffi.cdef("#if (1 << -1) == 0\n#endif\n") end,
         "shift count out of range")

print("ffi/164_preproc_if_unsigned ok")
