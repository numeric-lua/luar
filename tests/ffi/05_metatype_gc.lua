-- tests/ffi/05_metatype_gc.lua -- ffi.metatype and ffi.gc.

local H = dofile("tests/helpers.lua")
local ffi = require "ffi"

ffi.cdef [[
typedef struct Vec { double x, y; } Vec;
typedef struct Pos { int a, b; } Pos;
typedef struct Tagged { int n; } Tagged;
]]

-- Metatype index/tostring/call ----------------------------------------
local Vec = ffi.typeof("Vec")
local vecmt = {
  __index = {
    norm = function (self)
      return math.sqrt(self.x * self.x + self.y * self.y)
    end,
  },
  __tostring = function (self)
    return string.format("Vec(%g,%g)", self.x, self.y)
  end,
  __call = function (self, k)
    return self.x + k
  end,
}
H.check(rawequal(ffi.metatype(Vec, vecmt), Vec))
H.check(rawequal(ffi.metatype(Vec, vecmt), Vec))  -- same table is allowed

local v = ffi.new("Vec", {x = 3, y = 4})
H.eq(type(v), "cdata")
H.eq(v.x, 3)
H.eq(v:norm(), 5)
H.eq(tostring(v), "Vec(3,4)")
H.eq(v(2), 5)

-- Positional metatype constructors forward all arguments.
local Pos = ffi.metatype("Pos", {
  __index = { sum = function (self) return self.a + self.b end },
})
local p = Pos(5, 6)
H.eq(p.a, 5)
H.eq(p.b, 6)
H.eq(p:sum(), 11)
local p1 = Pos(9)
H.eq(p1.a, 9)
H.eq(p1.b, 0)
local p0 = Pos()
H.eq(p0.a, 0)
H.eq(p0.b, 0)
local pn = ffi.new(Pos, 1, 2)
H.eq(pn.a, 1)
H.eq(pn.b, 2)

-- __index/__newindex are field fallbacks, not overrides.
local writes = {}
local Tagged = ffi.typeof("Tagged")
ffi.metatype(Tagged, {
  __index = function (self, k) return "missing:" .. tostring(k) end,
  __newindex = function (self, k, val) writes[k] = val end,
})
local t = ffi.new("Tagged")
H.eq(t.n, 0)                  -- a real field: no metamethod
t.n = 3
H.eq(t.n, 3)
H.eq(t.other, "missing:other")
t.other = 7
H.eq(writes.other, 7)
H.eq(writes.n, nil)

H.raises(function() ffi.metatype(Vec, {}) end, "protected")
H.raises(function() ffi.metatype(ffi.typeof("int"), {}) end, "struct")
H.raises(function() ffi.gc(ffi.new("int"), 42) end, "function")
H.raises(function() ffi.gc(ffi.typeof("int"), function() end) end,
         "finalizer")

-- ffi.gc Lua finalizers ------------------------------------------------
local calls = {}
local x = ffi.new("int[1]")
local same = ffi.gc(x, function (cd)
  calls[#calls + 1] = type(cd)
end)
H.check(rawequal(same, x))
x = nil
same = nil
collectgarbage("collect")
collectgarbage("collect")
H.eq(#calls, 1)
H.eq(calls[1], "cdata")

local cleared = 0
local y = ffi.new("int[1]")
ffi.gc(y, function () cleared = cleared + 1 end)
ffi.gc(y, nil)                -- remove finalizer
y = nil
collectgarbage("collect")
collectgarbage("collect")
H.eq(cleared, 0)

print("ffi/05_metatype_gc ok")
