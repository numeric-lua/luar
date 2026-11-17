-- ffi/08_bitwise.lua -- LuaJIT-compatible cdata bitwise/shift/idiv/pow.
local ffi = require "ffi"
local H = dofile("tests/helpers.lua")

local i = ffi.new("int64_t", 5)
local m = ffi.new("int64_t", -1)
local u = ffi.new("uint64_t", 5)
local n = ffi.new("int", 5)
local d = ffi.new("double", 5.9)

-- elementary operators, LuaJIT result types
H.eq(i & 3, 1)
H.eq(i | 3, 7)
H.eq(i ~ 3, 6)
H.eq(~i, -6)
H.eq(i << 2, 20)
H.eq(i >> 1, 2)
H.eq(3 & i, 1)
H.eq(type(i & 3), "cdata")
H.eq(tostring(i & 3), "1LL")

-- unsigned promotion: either operand makes the result uint64
H.eq(u & 3, 1)
H.eq(tostring(u & 3), "1ULL")
H.eq(i & u, 5)
H.eq(tostring(i & u), "5ULL")
H.eq(tostring(~u), "18446744073709551610ULL")
H.eq(type(~u), "cdata")
H.eq(u >> 1, 2)
H.eq(u << 1, 10)

-- other numeric cdata promote to int64
H.eq(n & 3, 1)
H.eq(d & 3, 1)                 -- truncated like LuaJIT
H.eq(i << 1.7, 10)             -- count truncated to 1

-- 64-bit wrap, logical right shift, masked shift counts
H.eq(i << 63, math.mininteger)
H.eq(i << 65, 10)              -- 65 & 63 == 1
H.eq(i << 64, 5)               -- 64 & 63 == 0
H.eq(i << -1, math.mininteger) -- -1 & 63 == 63
H.eq(m >> 1, math.maxinteger)  -- logical, not arithmetic
H.eq(m >> 64, -1)              -- count masked to 0
H.eq(m << 1, -2)

-- integer division and power follow C/LuaJIT cdata rules
H.eq(i // 2, 2)
H.eq(m // 2, 0)                -- C truncation, not floor
H.eq(i ^ 2, 25)
H.eq(i ^ 0, 1)
H.eq(i ^ -1, 0)
H.eq(m ^ -1, -1)
H.eq(m ^ -2, 1)
H.eq(m ^ 2, 1)
H.eq(tostring(i ^ 2), "25LL")

-- non-numeric cdata and non-numbers are rejected, not interpreted
H.raises(function () return ffi.cast("void *", 8) & 1 end,
         "invalid operands")
H.raises(function () return true & i end, "invalid operands")
H.raises(function () return "x" & i end, "invalid operands")

print("ffi/08_bitwise ok")
