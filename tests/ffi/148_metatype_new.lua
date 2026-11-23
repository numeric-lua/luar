-- ffi/148_metatype_new.lua -- calling a CType object honors metatype __new.
--
-- LuaJIT dispatches __new only for `ctype(...)`; ffi.new and ffi.cast keep
-- the built-in constructor, so a handler may call them without recursing.
-- The handler receives the type object followed by the original arguments
-- and all of its results become the call's results.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/148_metatype_new: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")

ffi.cdef[[
typedef struct NewPoint { int x, y; } NewPoint;
typedef struct PlainPoint { int x; } PlainPoint;
]]

local seen_type, seen_argc, seen_arg
local mt = {
  __new = function (ct, ...)
    local n = ...
    seen_type = ct
    seen_argc = select("#", ...)
    seen_arg = n
    local o = ffi.new(ct, 0, 0)       -- must not re-enter __new
    o.x = (n or 0) + 100
    o.y = -1
    return o, "extra"
  end,
  __tostring = function (p) return "NewPoint(" .. p.x .. "," .. p.y .. ")" end,
}
H.eq(ffi.metatype("NewPoint", mt), ffi.typeof("NewPoint"))
local NP = ffi.typeof("NewPoint")

local a, tag = NP(7)
H.eq(type(a), "cdata")
H.eq(tostring(a), "NewPoint(107,-1)", "__new result")
H.eq(tag, "extra", "__new extra result")
H.check(rawequal(seen_type, NP), "__new receives the CType object")
H.eq(seen_argc, 1, "__new argument count")
H.eq(seen_arg, 7, "__new argument value")

-- Calling through another spelling of the same type is the same call.
local b = ffi.typeof("NewPoint")(-5)
H.eq(tostring(b), "NewPoint(95,-1)")

-- No arguments: the handler still runs and may default them.
local c = NP()
H.eq(tostring(c), "NewPoint(100,-1)")

-- ffi.new and ffi.cast keep the built-in constructor.
local d = ffi.new("NewPoint", 3, 4)
H.eq(d.x, 3, "ffi.new bypasses __new x")
H.eq(d.y, 4, "ffi.new bypasses __new y")
local e = ffi.new(NP, 8, 9)
H.eq(e.x, 8, "ffi.new(CType) bypasses __new")
H.eq(e.y, 9, "ffi.new(CType) bypasses __new y")

-- A handler error propagates unchanged.
do
  ffi.cdef[[typedef struct NewError { int x; } NewError; ]]
  ffi.metatype("NewError", { __new = function () error("new failed") end })
  H.raises(function () return ffi.typeof("NewError")() end, "new failed")
end

-- Without __new the built-in constructor is used.
do
  local p = ffi.typeof("PlainPoint")(5)
  H.eq(p.x, 5, "fallback constructor")
end

-- The metatype table itself is not modified by the call.
H.eq(mt.__new ~= nil, true)

print("ffi/148_metatype_new ok")
