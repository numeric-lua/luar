-- ffi/108_metatype_relational.lua -- metatype __eq/__lt/__le/__len dispatch.
--
-- LuaJIT invokes these metamethods for struct/union cdata that carry a
-- metatype.  Other cdata (numbers, pointers, arrays) keep the built-in
-- FFI comparison and length rules.
local H = dofile("tests/helpers.lua")
local ffi = require "ffi"

ffi.cdef[[
typedef struct Pair { int x, y; } Pair;
typedef struct Unmeta { int x; } Unmeta;
typedef struct Numbered { int n; } Numbered;
]]

local calls = {}
local function note (name)
  calls[#calls + 1] = name
end

local Pair = ffi.metatype("Pair", {
  __eq = function (a, b) note("eq"); return a.x == b.x and a.y == b.y end,
  __lt = function (a, b) note("lt"); return a.x < b.x end,
  __le = function (a, b) note("le"); return a.x <= b.x end,
  __len = function (a) note("len"); return a.x + a.y end,
})

local a = ffi.new("Pair", 1, 2)
local b = ffi.new("Pair", 1, 2)
local c = ffi.new("Pair", 3, 4)

H.eq(a == b, true)
H.eq(calls[#calls], "eq")
H.eq(a == c, false)
H.eq(a ~= c, true)
H.eq(calls[#calls], "eq")

H.eq(a < c, true)
H.eq(calls[#calls], "lt")
H.eq(c > a, true)
H.eq(calls[#calls], "lt")
H.eq(a <= b, true)
H.eq(calls[#calls], "le")
H.eq(c >= b, true)
H.eq(calls[#calls], "le")

H.eq(#a, 3)
H.eq(calls[#calls], "len")
H.eq(#c, 7)

-- A handler on either operand is used (the right-hand type here).
local u = ffi.new("Unmeta", 9)
H.eq(a == u, false)          -- __eq compares fields through the Pair handler
H.eq(a < u, true)            -- 1 < 9
H.eq(u > a, true)            -- swapped __lt call: 9 > 1
H.eq(u <= a, false)          -- 9 <= 1

-- Structs without a metatype keep raw cdata semantics: identity equality
-- and no ordering.
local u1 = ffi.new("Unmeta", 1)
local u2 = ffi.new("Unmeta", 1)
H.eq(u1 == u2, false)
H.eq(u1 == u1, true)
H.raises(function () return u1 < u2 end, "compare cdata")

-- Numeric cdata still uses the built-in numeric comparison.
local n1 = ffi.new("int64_t", 5)
local n2 = ffi.new("int64_t", 6)
H.eq(n1 < n2, true)
H.eq(n1 == ffi.new("int64_t", 5), true)

-- __len is only used when the metatype supplies it.
ffi.metatype("Numbered", { })
local z = ffi.new("Numbered", 4)
H.raises(function () return #z end, "length")

print("ffi/108_metatype_relational ok")
