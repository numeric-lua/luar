-- ffi/115_metatype_gc.lua -- a metatype __gc is the default finalizer.
--
-- LuaJIT runs a struct metatype's __gc when the cdata is collected, unless
-- ffi.gc() was called on the object.  An explicit ffi.gc(x, nil) also
-- overrides the metatype finalizer.  Errors in either are ignored.
local H = dofile("tests/helpers.lua")
local ffi = require "ffi"

ffi.cdef [[
typedef struct G { int n; } G;
typedef struct H { int n; } H;
typedef struct E { int n; } E;
]]

local events = {}
local G = ffi.metatype("G", {
  __gc = function (self)
    events[#events + 1] = "meta:" .. tostring(self.n)
  end,
})

-- Default metatype finalizer.
do local x = G(7); H.eq(x.n, 7) end
collectgarbage("collect")
collectgarbage("collect")
H.eq(#events, 1, "metatype __gc did not run once")
H.eq(events[1], "meta:7")

-- An explicit Lua finalizer overrides it.
events = {}
do
  local x = G(8)
  ffi.gc(x, function () events[#events + 1] = "gc" end)
end
collectgarbage("collect")
collectgarbage("collect")
H.eq(#events, 1, "ffi.gc should override metatype __gc")
H.eq(events[1], "gc")

-- ffi.gc(x, nil) also suppresses the metatype finalizer.
events = {}
do
  local x = G(9)
  ffi.gc(x, nil)
end
collectgarbage("collect")
collectgarbage("collect")
H.eq(#events, 0, "ffi.gc(x, nil) must suppress metatype __gc")

-- A type without __gc keeps the old no-op behavior.
local HT = ffi.metatype("H", {})
do local x = HT(1) end
collectgarbage("collect")
collectgarbage("collect")
H.eq(#events, 0, "unexpected finalizer")

-- Errors from metatype __gc are ignored and do not stop collection.
local E = ffi.metatype("E", {
  __gc = function () error("finalizer boom") end,
})
do local x = E(1) end
collectgarbage("collect")
collectgarbage("collect")
H.check(type(G) == "cdata" and type(E) == "cdata", "types survived")

-- Numeric cdata has no metatype; making one must still fail.
H.raises(function () ffi.metatype("int64_t", {}) end, "struct")
H.raises(function () ffi.metatype("G", {}) end, "protected")

print("ffi/115_metatype_gc ok")
