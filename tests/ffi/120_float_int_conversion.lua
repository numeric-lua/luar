-- ffi/120_float_int_conversion.lua -- out-of-range double-to-integer
-- conversions are defined and match LuaJIT's x86-64 sentinel.
--
-- A C conversion is undefined when the double is outside the destination
-- range.  Compilers are allowed to assume the range and fold the result
-- differently (GCC did for some constants), so Luar now defines the
-- LuaJIT-observable behaviour: signed out-of-range -> INT64_MIN, unsigned
-- values in [2^63, 2^64) stay exact, and everything else above the range
-- or NaN -> 2^63.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/120_float_int_conversion: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")

local inf = math.huge
local nan = 0 / 0

local function i64 (v) return tostring(ffi.cast("int64_t", v)) end
local function u64 (v) return tostring(ffi.cast("uint64_t", v)) end

-- Signed conversions: in-range values truncate, everything else saturates
-- to the INT64_MIN sentinel.
H.eq(i64(-inf), "-9223372036854775808LL", "int64 -inf")
H.eq(i64(inf), "-9223372036854775808LL", "int64 +inf")
H.eq(i64(nan), "-9223372036854775808LL", "int64 nan")
H.eq(i64(-1e300), "-9223372036854775808LL", "int64 -1e300")
H.eq(i64(1e300), "-9223372036854775808LL", "int64 1e300")
H.eq(i64(-1e19), "-9223372036854775808LL", "int64 -1e19")
H.eq(i64(1e19), "-9223372036854775808LL", "int64 1e19")
H.eq(i64(-9223372036854775808.0), "-9223372036854775808LL",
     "int64 exact minimum")
H.eq(i64(9223372036854774784.0), "9223372036854774784LL",
     "int64 below 2^63")
H.eq(i64(9223372036854775808.0), "-9223372036854775808LL",
     "int64 2^63")
H.eq(i64(-1.9), "-1LL", "int64 truncates towards zero")
H.eq(i64(1.9), "1LL", "int64 truncates towards zero (positive)")

-- Unsigned conversions: [2^63, 2^64) is exact; negative values wrap like
-- the corresponding signed conversion; NaN and >=2^64 use the 2^63
-- sentinel.
H.eq(u64(-inf), "9223372036854775808ULL", "uint64 -inf")
H.eq(u64(inf), "9223372036854775808ULL", "uint64 +inf")
H.eq(u64(nan), "9223372036854775808ULL", "uint64 nan")
H.eq(u64(-1.9), "18446744073709551615ULL", "uint64 negative wraps")
H.eq(u64(-1e19), "9223372036854775808ULL", "uint64 very negative")
H.eq(u64(1e19), "10000000000000000000ULL", "uint64 1e19")
H.eq(u64(18446744073709549568.0), "18446744073709549568ULL",
     "uint64 2^64 - 2048")
H.eq(u64(18446744073709551616.0), "0ULL", "uint64 2^64 wraps to zero")
H.eq(u64(18446744073709551616.0 + 2^40), "1099511627776ULL",
     "uint64 above 2^64 keeps the low part")
H.eq(u64(1.5 * 18446744073709551616.0), "9223372036854775808ULL",
     "uint64 1.5*2^64 sentinel")
H.eq(u64(1e30), "9223372036854775808ULL", "uint64 1e30 sentinel")

-- ffi.new and assignment go through the same conversion path; narrowed
-- destinations keep only the low bytes of the (now defined) 64-bit value.
H.eq(tostring(ffi.new("int64_t", inf)), "-9223372036854775808LL",
     "ffi.new int64 inf")
H.eq(tostring(ffi.new("uint64_t", 1e19)), "10000000000000000000ULL",
     "ffi.new uint64 1e19")
H.eq(tonumber(ffi.new("int32_t", 1e30)), 0, "ffi.new int32 sentinel low bits")
H.eq(tonumber(ffi.new("int8_t", -inf)), 0, "ffi.new int8 sentinel low bits")
H.eq(tonumber(ffi.new("uint8_t", 1e30)), 0, "ffi.new uint8 sentinel low bits")

local a = ffi.new("int8_t[2]")
a[0] = inf
a[1] = -inf
H.eq(a[0], 0, "array store +inf")
H.eq(a[1], 0, "array store -inf")

-- Arithmetic and comparison promote the double through the same signed
-- conversion, so the sentinel participates instead of an undefined value.
H.eq(tostring(ffi.new("double", 1e30) + ffi.new("int64_t", 1)),
     "-9223372036854775807LL", "arithmetic uses signed sentinel")
H.eq(tostring(ffi.new("double", 1e30) - ffi.new("int64_t", 1)),
     "9223372036854775807LL", "subtraction uses signed sentinel")
H.check(ffi.new("double", 1e30) < ffi.new("int64_t", 0),
        "comparison uses signed sentinel")
H.check(ffi.new("double", 1e30) == ffi.new("int64_t", -9223372036854775808),
        "equality compares the signed sentinel")

print("ffi/120_float_int_conversion ok")
