-- tests/ffi/04_cdata.lua -- cdata creation, conversion, aggregate access,
-- pointer arithmetic, int64 semantics and memory helpers.

local H = dofile("tests/helpers.lua")
local ffi = require "ffi"

ffi.cdef [[
typedef struct DataP { int x; int y; } DataP;
struct DataQ { DataP p; int a[2]; };
typedef struct DataU { int kind; union { int i; double d; }; } DataU;
]]

-- Scalars -------------------------------------------------------------
H.eq(tonumber(ffi.new("int")), 0)
local i = ffi.new("int", 42)
H.eq(type(i), "cdata")
H.eq(tonumber(i), 42)
H.eq(i == 42, true)
H.eq(42 == i, true)
H.eq(i + 1 == 43, true)
H.eq(tostring(i + 1), "43LL")
H.eq(tonumber(ffi.new("uint8_t", 300)), 44)
H.eq(tonumber(ffi.new("int", 3.9)), 3)
H.eq(tonumber(ffi.new("double", 1.5)), 1.5)
H.eq(tonumber(ffi.new("double", 2)), 2.0)
H.eq(tonumber(ffi.new("bool", true)), 1)
H.eq(tonumber(ffi.new("bool", false)), 0)

H.eq(tonumber(ffi.cast("int", 5.9)), 5)
H.eq(tonumber(ffi.cast("uint8_t", -1)), 255)
H.eq(tonumber(ffi.cast("int64_t", 7)), 7)
H.eq(tostring(ffi.cast("uint64_t", -1)), "18446744073709551615ULL")
local z = ffi.cast("void *", 0)
H.check(z == nil)
H.check(tostring(z):find("NULL", 1, true) ~= nil)

-- int64/uint64 conversion, comparison and wraparound ------------------
local max = ffi.new("int64_t", 0x7fffffffffffffff)
H.eq(tostring(max), "9223372036854775807LL")
H.eq(tostring(max + 1), "-9223372036854775808LL")
H.eq(tostring(ffi.new("uint64_t", 0) - 1), "18446744073709551615ULL")
H.eq(tostring(ffi.new("int64_t", -1)), "-1LL")
H.eq(tostring(ffi.new("uint64_t", -1)), "18446744073709551615ULL")
H.eq(ffi.new("int64_t", 5) == ffi.new("uint64_t", 5), true)
H.eq(ffi.new("int64_t", 5) < ffi.new("uint64_t", 5), false)
H.eq(ffi.new("uint64_t", 5) + 1 == ffi.new("uint64_t", 6), true)
H.eq(ffi.new("int64_t", 5) + 1 == ffi.new("int64_t", 6), true)
H.eq(ffi.new("int64_t", 5) == 5, true)
H.eq(ffi.new("int64_t", 5) == 5.0, true)
H.eq(ffi.new("int64_t", 5) == 5.5, true)   -- LuaJIT truncates the number
H.eq(ffi.new("int64_t", 5) < 6, true)
H.eq(ffi.new("int64_t", 5) <= 5, true)
H.eq(4 < ffi.new("int64_t", 5), true)
H.eq(tonumber(ffi.new("int64_t", -7)), -7)
H.eq(tonumber(ffi.new("uint64_t", 42)), 42)
H.eq(tonumber(ffi.new("double", 1.5)), 1.5)

-- Arrays --------------------------------------------------------------
local a = ffi.new("int[3]", {1, 2, 3})
H.eq(a[0], 1)
H.eq(a[1], 2)
H.eq(a[2], 3)
a[1] = 20
H.eq(a[1], 20)
H.eq(ffi.sizeof(a), 12)

local a0 = ffi.new("int[3]", {[0] = 10, [1] = 20, [2] = 30})
H.eq(a0[0], 10)
H.eq(a0[1], 20)
H.eq(a0[2], 30)

local rep = ffi.new("int[3]", {7})
H.eq(rep[0], 7)
H.eq(rep[1], 7)
H.eq(rep[2], 7)

local av = ffi.new("int[4]", 1, 2, 3, 4)
H.eq(av[0], 1)
H.eq(av[3], 4)
local avr = ffi.new("int[3]", 7)
H.eq(avr[0], 7)
H.eq(avr[2], 7)

local v = ffi.new("int[?]", 4, {1, 2, 3, 4})
H.eq(ffi.sizeof(v), 16)
H.eq(v[3], 4)
H.raises(function() return v[4] end, "out of bounds")
H.raises(function() return v[-1] end, "out of bounds")

local s8 = ffi.new("char[8]", "hi")
H.eq(ffi.string(s8), "hi")
H.eq(#ffi.string(s8, 8), 8)
local sv = ffi.new("char[?]", 8, "abc")
H.eq(ffi.string(sv), "abc")
H.eq(#ffi.string(sv, 8), 8)
H.eq(ffi.sizeof(sv), 8)
H.eq(ffi.new("char[4]", "abcdef")[3], 100)  -- 'd': truncated, no overflow

-- Structs, nested initializers and aggregate assignment ---------------
local p = ffi.new("DataP", 3, 4)
H.eq(p.x, 3)
H.eq(p.y, 4)
p.x = 10
H.eq(p.x, 10)
local p1 = ffi.new("DataP", 9)
H.eq(p1.x, 9)
H.eq(p1.y, 0)
local p2 = ffi.new("DataP", {y = 8})
H.eq(p2.x, 0)
H.eq(p2.y, 8)
local pcopy = ffi.new("DataP", p)
H.eq(pcopy.x, 10)
H.eq(pcopy.y, 4)

local q = ffi.new("struct DataQ", {p = {x = 1, y = 2}, a = {3, 4}})
H.eq(q.p.x, 1)
H.eq(q.p.y, 2)
H.eq(q.a[0], 3)
H.eq(q.a[1], 4)
q.p = {x = 7, y = 8}
H.eq(q.p.x, 7)
q.a[1] = 9
H.eq(q.a[1], 9)
local q2 = ffi.new("struct DataQ", {{1, 2}, {3, 4}})
H.eq(q2.p.x, 1)
H.eq(q2.p.y, 2)
H.eq(q2.a[0], 3)
H.eq(q2.a[1], 4)
q2.a = {11, 12}
H.eq(q2.a[0], 11)
H.eq(q2.a[1], 12)

local u = ffi.new("DataU", {kind = 1, i = 7})
H.eq(u.kind, 1)
H.eq(u.i, 7)
u.i = 9
H.eq(u.i, 9)

H.raises(function() return ffi.new("DataP").missing end, "no member")
H.raises(function() ffi.new("DataP").x = "x" end, "cannot convert")
H.raises(function() ffi.new("DataP", 1, 2, 3) end, "too many")
H.raises(function() return ffi.new("int", 1, 2) end, "too many")

-- Pointers, casts and pointer arithmetic ------------------------------
local arr = ffi.new("int[4]", {10, 20, 30, 40})
local ptr = ffi.cast("int *", arr)
H.eq(ptr[0], 10)
H.eq(ptr[2], 30)
H.eq((ptr + 1)[0], 20)
ptr[1] = 99
H.eq(arr[1], 99)
local d = (ptr + 2) - ptr
H.eq(tonumber(d), 2)
H.eq(d == 2, true)
H.check(ptr == ffi.cast("int *", arr))
H.check(ptr ~= ffi.cast("int *", arr) + 1)
H.check(ptr < ptr + 1)
H.check(ptr + 1 <= ptr + 1)
local np = ffi.cast("int *", 0)
H.check(np == nil)
H.check(nil == np)
H.raises(function() return np[0] end, "NULL pointer")
local nf = ffi.cast("void (*)(void)", 0)
H.check(nf == nil)
H.raises(function() return np < nil end, "compare")

-- Taking the address of an aggregate cdata via ffi.cast.
local pp = ffi.new("DataP", 1, 2)
local pptr = ffi.cast("DataP *", pp)
H.eq(pptr[0].x, 1)
pptr[0].y = 5
H.eq(pp.y, 5)
local pcopy2 = ffi.cast("DataP", pp)
H.eq(pcopy2.x, 1)
H.eq(pcopy2.y, 5)

-- Strings, copy and fill ---------------------------------------------
local b = ffi.new("char[16]")
ffi.fill(b, 16, 65)
H.eq(ffi.string(b, 16), string.rep("A", 16))
ffi.copy(b, "hello")
H.eq(ffi.string(b), "hello")
H.eq(b[5], 0)
H.eq(#ffi.string(b, 6), 6)
ffi.copy(b, "hi", 3)                 -- LuaJIT allows len == #src + 1
H.eq(ffi.string(b), "hi")
H.eq(b[2], 0)
local src = ffi.new("char[4]", "abc")
local dst = ffi.new("char[4]")
ffi.copy(dst, src, 4)
H.eq(#ffi.string(dst, 4), 4)
H.eq(dst[0], 97)

H.eq(ffi.string(ffi.new("char *", "abc")), "abc")
H.eq(ffi.string(ffi.cast("const char *", "abc")), "abc")
H.eq(ffi.string(ffi.new("char[?]", 4, "xyz")), "xyz")
H.eq(ffi.string(ffi.new("char[5]", "hello"), 5), "hello")
H.eq(ffi.string(ffi.new("char[?]", 4, "abcd"), 4), "abcd")

-- tostring formats ----------------------------------------------------
H.eq(tostring(ffi.typeof("int")), "ctype<int>")
H.eq(tostring(ffi.new("int64_t", 5)), "5LL")
H.eq(tostring(ffi.new("uint64_t", 5)), "5ULL")
H.check(tostring(ffi.NULL) == "nil")
H.check(tostring(np):find("cdata<int *>: NULL", 1, true) ~= nil)
H.check(tostring(ptr):find("cdata<int *>: 0x", 1, true) ~= nil)
local pd = ffi.new("DataP")
H.check(tostring(pd):find("cdata<struct DataP>: 0x", 1, true) ~= nil)

-- Errors --------------------------------------------------------------
H.raises(function() return ffi.string(ffi.new("int")) end, "char pointer")
H.raises(function() ffi.copy(ffi.new("char[2]"), "hello") end, "out of bounds")
H.raises(function() ffi.copy(ffi.new("char[8]"), "hi", 4) end, "shorter")
H.raises(function() ffi.fill(ffi.new("char[2]"), 3) end, "out of bounds")
H.raises(function() return ffi.new("void") end, "void")
H.raises(function() return ffi.cast("int", "x") end, "cannot convert")
H.raises(function() return ffi.new("char[?]", 4, "ab")[4] end, "out of bounds")

print("ffi/04_cdata ok")
