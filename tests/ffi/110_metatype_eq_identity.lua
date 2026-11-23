-- ffi/110_metatype_eq_identity.lua -- '__eq' runs for identical cdata.
--
-- LuaJIT dispatches a struct/union metatype's '__eq' even when both
-- operands are the same cdata object.  Stock Lua short-circuits identical
-- userdata, so Luar's FFI hook must route the self-comparison through the
-- handler; rawequal() stays raw.
local H = dofile("tests/helpers.lua")
local ffi = require "ffi"

ffi.cdef[[
typedef struct EqA { int x; } EqA;
typedef struct EqB { int x; } EqB;
typedef struct EqC { int x; } EqC;
]]

local calls = 0

local EqA = ffi.metatype("EqA", {
  __eq = function (a, b)
    calls = calls + 1
    return false
  end,
})

local EqC = ffi.metatype("EqC", {
  __eq = function (a, b)
    calls = calls + 1
    return true
  end,
})

-- A metatype without '__eq' keeps raw identity.
ffi.metatype("EqB", { })

local a = ffi.new("EqA", 1)
local c = ffi.new("EqC", 2)
local b = ffi.new("EqB", 3)

-- Self-comparison goes through the handler and uses its result.
H.eq(a == a, false)
H.eq(calls, 1)
H.eq(a ~= a, true)
H.eq(calls, 2)
H.eq(c == c, true)
H.eq(calls, 3)

-- rawequal() is a raw identity test and must not invoke the metamethod.
H.eq(rawequal(a, a), true)
H.eq(rawequal(a, c), false)
H.eq(calls, 3)

-- Different objects still dispatch normally.
H.eq(a == ffi.new("EqA", 1), false)
H.eq(calls, 4)

-- No '__eq' handler: identity is true, distinct objects are false.
H.eq(b == b, true)
H.eq(b == ffi.new("EqB", 3), false)
H.eq(calls, 4)

-- Scalar cdata without a metatype keeps its built-in comparison.
local n = ffi.new("int", 1)
H.eq(n == n, true)

print("ffi/110_metatype_eq_identity ok")
