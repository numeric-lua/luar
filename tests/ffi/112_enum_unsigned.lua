-- ffi/112_enum_unsigned.lua -- enum numeric conversions use the unsigned
-- 32-bit view, as in LuaJIT.
--
-- An enum cdata constructed with -1 keeps the signed storage value (and
-- prints as -1), but LuaJIT converts it to 4294967295 for tonumber,
-- arithmetic, casts and comparisons.  Treating it as a sign-extended
-- int64_t made `e == 0xffffffff` false and produced -1 in 64-bit results.
local H = dofile("tests/helpers.lua")

local ffi = require "ffi"

ffi.cdef [[
typedef enum { EU_A = 1, EU_B = 5 } EU;
typedef struct { EU e; int tag; } EUHolder;
]]

local function new_e (v)
  return ffi.new("EU", v)
end

-- Storage and display stay signed.
local e = new_e(-1)
H.check(tostring(e):find("%-1") ~= nil, "enum display is not signed")
H.eq(ffi.sizeof("EU"), 4, "enum size")

-- Numeric conversion is the unsigned 32-bit value.
H.eq(tonumber(e), 4294967295, "tonumber(enum -1)")
H.eq(tonumber(ffi.new("EU", 0xffffffff)), 4294967295,
     "tonumber(enum 0xffffffff)")
H.eq(tonumber(e + 0), 4294967295, "enum + 0")
H.eq(tonumber(e - new_e(1)), 4294967294, "enum - enum")
H.eq(tonumber(e + new_e(1)), 4294967296, "enum wrap into 64-bit result")
H.eq(e == 0xffffffff, true, "enum equality with the unsigned value")
H.eq(e == -1, false, "enum equality must not sign-extend")
H.eq(e < 0, false, "enum ordering must not sign-extend")
H.eq(e > new_e(1), true, "enum ordering against a small value")

-- Casts follow the same conversion; narrowing back to int recovers -1.
H.eq(tonumber(ffi.cast("int64_t", e)), 4294967295, "cast enum to int64_t")
H.eq(tonumber(ffi.cast("uint64_t", e)), 4294967295, "cast enum to uint64_t")
H.eq(tonumber(ffi.cast("double", e)), 4294967295.0, "cast enum to double")
H.eq(tonumber(ffi.cast("int", e)), -1, "cast enum to int")
H.eq(tonumber(ffi.cast("uint8_t", e)), 255, "cast enum to uint8_t")

-- Struct fields use the same numeric view.
local s = ffi.new("EUHolder")
s.e = -1
H.eq(s.e, 4294967295, "enum struct field")
H.eq(s.e + 0, 4294967295, "enum struct field arithmetic")
H.eq(tonumber(s.e), 4294967295, "enum struct field tonumber")

-- Assignment still truncates to the 32-bit storage unit.
s.e = 4294967295
H.eq(s.e, 4294967295, "assigning the unsigned form")
H.eq(tostring(s.e), tostring(4294967295), "field does not wrap to -1")

-- Passing an enum where an integer is declared keeps the low 32 bits.
local h = dofile("tests/ffi/support.lua").helper()
ffi.cdef[[int helper_color_value(EU c);]]
H.eq(h.helper_color_value(e), -1, "enum argument keeps its 32-bit bits")

print("ffi/112_enum_unsigned ok")
