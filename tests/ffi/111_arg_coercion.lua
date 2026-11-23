-- ffi/111_arg_coercion.lua -- FFI integer arguments use C conversion rules.
--
-- LuaJIT truncates a finite Lua float towards zero for lengths, byte values,
-- array indices and ffi.errno, and rejects strings.  Lua 5.5's
-- luaL_checkinteger did the opposite (reject 1.9, accept "1"), which made
-- these FFI entry points surprising and ABI-inconsistent.  NaN, infinity
-- and out-of-range values are rejected rather than invoking an undefined C
-- float-to-integer conversion.
local H = dofile("tests/helpers.lua")
local ffi = require "ffi"

-- ffi.fill: fractional length and byte truncate; strings are rejected.
local b = ffi.new("uint8_t[8]")
ffi.fill(b, 2.9, 257.9)
H.eq(b[0], 1, "fill byte did not truncate and wrap")
H.eq(b[1], 1, "fill length did not truncate")
H.eq(b[2], 0, "fill wrote past the truncated length")

ffi.fill(b, 1, -1.9)
H.eq(b[0], 255, "negative fill byte did not use its low 8 bits")

H.raises(function () ffi.fill(b, 1, "65") end, "must be a number")
H.raises(function () ffi.fill(b, "2", 65) end, "must be a number")
H.raises(function () ffi.fill(b, 0 / 0, 65) end, "out of range")
H.raises(function () ffi.fill(b, 1 / 0, 65) end, "out of range")

-- ffi.copy / ffi.string lengths.
local src = ffi.new("char[8]", "abc")
local dst = ffi.new("char[8]")
ffi.copy(dst, src, 2.9)
H.eq(dst[0], string.byte("a"))
H.eq(dst[1], string.byte("b"))
H.eq(dst[2], 0, "copy wrote past the truncated length")
ffi.copy(dst, "abc")
H.eq(ffi.string(dst), "abc", "copy default length changed")

H.eq(ffi.string(src, 2.9), "ab", "string length did not truncate")
H.eq(ffi.string(src), "abc", "string default length changed")
H.raises(function () ffi.copy(dst, src, "2") end, "must be a number")
H.raises(function () ffi.string(src, "2") end, "must be a number")

-- ffi.sizeof / ffi.new variable-length array length.
local isz = ffi.sizeof("int")
H.eq(ffi.sizeof("int[?]", 2.9), 2 * isz, "sizeof VLA length did not truncate")
local a = ffi.new("int[?]", 2.9)
H.eq(ffi.sizeof(a), 2 * isz, "ffi.new VLA length did not truncate")
H.raises(function () return ffi.sizeof("int[?]", "2") end,
         "must be a number")
H.raises(function () return ffi.new("int[?]", "2") end,
         "must be a number")

-- Array indices truncate towards zero; non-finite indices are rejected.
local ai = ffi.new("int[4]", 10, 20, 30, 40)
H.eq(ai[1.9], 20, "fractional array index did not truncate")
H.eq(ai[-0.5], 10, "negative fractional index did not truncate to zero")
ai[2.9] = 99
H.eq(ai[2], 99, "fractional array store index did not truncate")
H.raises(function () return ai[0 / 0] end, "out of range")

-- ffi.errno accepts a finite float and rejects a string.
ffi.errno(0)
H.eq(ffi.errno(1.9), 0, "errno did not accept a fractional value")
H.eq(ffi.errno(3), 1, "errno did not truncate the new value")
H.raises(function () return ffi.errno("2") end, "must be a number")
ffi.errno(0)

print("ffi/111_arg_coercion ok")
