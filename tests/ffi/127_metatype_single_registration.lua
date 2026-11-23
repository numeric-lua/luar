-- ffi/127_metatype_single_registration.lua -- a struct/union has exactly one
-- metatype, and any second ffi.metatype call raises.
--
-- The same-table case used to be accepted silently, unlike LuaJIT.  That
-- made a repeated registration look successful while the first metatable
-- remained in effect.  Both spellings of the type (string and ctype object)
-- must behave the same way, and the original metatable must stay installed.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/127_metatype_single_registration: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")

ffi.cdef[[
typedef struct { int v; } SingleMeta;
typedef union { int v; double d; } SingleMetaU;
typedef struct { int v; } OtherMeta;
]]

local mt = { __tostring = function(self) return "single:" .. self.v end }
local ct = ffi.metatype("SingleMeta", mt)
H.check(rawequal(ct, ffi.typeof("SingleMeta")),
        "metatype did not return the type object")

local value = ffi.new("SingleMeta", { v = 7 })
H.eq(tostring(value), "single:7", "metatype was not applied")

-- Repeating with the same table is an error, not a no-op.
H.raises(function() ffi.metatype("SingleMeta", mt) end, "protected")
-- Repeating through the ctype object is the same registration.
H.raises(function() ffi.metatype(ct, mt) end, "protected")
-- Trying to replace it with another table is an error too.
H.raises(function() ffi.metatype("SingleMeta", {}) end, "protected")

-- The first metatable is still the installed one after the rejected calls.
H.eq(tostring(value), "single:7", "a rejected replacement changed the metatype")
H.eq(tostring(ffi.new("SingleMeta", { v = 9 })), "single:9",
     "a rejected replacement broke later construction")

-- Unions follow the same rule.
local umt = { __tostring = function(self) return "union:" .. self.v end }
ffi.metatype("SingleMetaU", umt)
H.eq(tostring(ffi.new("SingleMetaU", { v = 3 })), "union:3")
H.raises(function() ffi.metatype("SingleMetaU", umt) end, "protected")

-- An unrelated type can still get its own first metatype.
local omt = { __tostring = function() return "other" end }
ffi.metatype("OtherMeta", omt)
H.eq(tostring(ffi.new("OtherMeta", { v = 1 })), "other",
     "a fresh type could not register a metatype")
print("ffi/127_metatype_single_registration ok")
