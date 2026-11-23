-- ffi/128_no_metatype_dispatch.lua -- __len/__pairs on a plain struct must
-- fail with the cdata message, not by indexing a missing metatable.
--
-- meta_push read registry[LUA_NOREF] when the struct/union had no metatype.
-- That returned nil, and the following lua_getfield raised "attempt to index
-- a nil value" from #cdata and pairs(cdata).  The intended errors are used
-- when a metatype is present or absent, and a hosted metatype still works.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/128_no_metatype_dispatch: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")

ffi.cdef[[
typedef struct { int x; } PlainLenS;
typedef union { int x; } PlainLenU;
typedef struct { int x; } MetaLenS;
]]

local s = ffi.new("PlainLenS")
local u = ffi.new("PlainLenU")
local sp = ffi.cast("PlainLenS *", s)

H.raises(function() return #s end, "length of a cdata value",
         "plain struct length")
H.raises(function() return #u end, "length of a cdata value",
         "plain union length")
H.raises(function() return #sp end, "length of a cdata value",
         "plain struct pointer length")
H.raises(function() return pairs(s) end, "iterate a cdata value",
         "plain struct pairs")
H.raises(function() return pairs(u) end, "iterate a cdata value",
         "plain union pairs")

-- Non-method paths still work without a metatype.
H.eq(type(tostring(s)), "string", "tostring on a plain struct")
H.eq(s.x, 0, "plain field read")

-- A metatype-provided __len and __pairs still dispatch.
ffi.metatype("MetaLenS", {
  __len = function() return 42 end,
  __pairs = function(self)
    local done = false
    return function()
      if done then return nil end
      done = true
      return "x", self.x
    end
  end,
})
local m = ffi.new("MetaLenS", { x = 5 })
H.eq(#m, 42, "metatype __len")
local key, value = pairs(m)()
H.eq(key, "x", "metatype __pairs key")
H.eq(value, 5, "metatype __pairs value")

print("ffi/128_no_metatype_dispatch ok")
