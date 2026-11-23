-- ffi/118_cdata_int_args.lua -- integer cdata is a valid C integer argument.
--
-- LuaJIT accepts numeric cdata wherever its FFI expects a C integer:
-- lengths, indices, errno, fill bytes and copy counts.  Luar used to reject
-- all of them with "must be a number", even though the same cdata converts
-- fine for every other FFI entry point.  Float cdata is accepted for
-- lengths (truncated, like a Lua float) but rejected as an array index,
-- matching LuaJIT.
local H = dofile("tests/helpers.lua")
local ffi = require "ffi"

local i3 = ffi.new("int", 3)
local u3 = ffi.new("uint64_t", 3)
local d3 = ffi.new("double", 3.9)
local btrue = ffi.new("bool", true)

H.eq(ffi.sizeof("int[?]", i3), 12)
H.eq(ffi.sizeof("int[?]", u3), 12)
H.eq(ffi.sizeof("int[?]", d3), 12, "float cdata truncates towards zero")
H.eq(ffi.sizeof("int[?]", btrue), 4, "bool cdata is an integer argument")

local arr = ffi.new("int[4]", 10, 20, 30, 40)
H.eq(arr[i3], 40)
H.eq(arr[ffi.new("int64_t", 1)], 20)
arr[ffi.new("int", 1)] = 99
H.eq(arr[1], 99)
arr[ffi.new("int64_t", 2)] = 77
H.eq(arr[2], 77)

local buf = ffi.new("char[8]", "abcdefg")
H.eq(ffi.string(buf, i3), "abc")
H.eq(ffi.string(buf, d3), "abc")

local copy = ffi.new("char[8]")
ffi.copy(copy, buf, ffi.new("int64_t", 3))
H.eq(ffi.string(copy), "abc")

local fill = ffi.new("char[4]")
ffi.fill(fill, ffi.new("int", 4), ffi.new("int", 65))
H.eq(ffi.string(fill), "AAAA")

local old = ffi.errno(ffi.new("int", 42))
H.eq(old, 0)
H.eq(ffi.errno(), 42)

H.raises(function () return arr[d3] end, "bad index",
         "float cdata must not index an array")
H.raises(function () return arr[btrue] end, "bad index",
         "bool cdata must not index an array")
H.raises(function () return ffi.sizeof("int[?]", "3") end, "must be a number",
         "numeric strings remain rejected")
H.raises(function () return ffi.sizeof("int[?]", ffi.typeof("int")) end,
         "must be a number", "a CType is not an integer argument")
H.raises(function () return ffi.sizeof("int[?]", ffi.new("double", 0/0)) end,
         "out of range", "NaN stays rejected")
H.raises(function () return ffi.new("int[?]", ffi.cast("uint64_t", -1)) end,
         "negative", "out-of-range cdata stays rejected")

print("ffi/118_cdata_int_args ok")
