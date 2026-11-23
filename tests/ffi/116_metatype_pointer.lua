-- ffi/116_metatype_pointer.lua -- pointer-to-struct metatype forwarding.
--
-- LuaJIT resolves __tostring, __len and __pairs through a pointer to the
-- struct/union that owns the metatype.  Luar previously forwarded __index,
-- __newindex and __call for pointers but not these three.
local H = dofile("tests/helpers.lua")
local ffi = require "ffi"

ffi.cdef [[
typedef struct LuarMetaPtrP { int n; } LuarMetaPtrP;
typedef struct LuarMetaPtrQ { int n; } LuarMetaPtrQ;
]]

local P = ffi.metatype("LuarMetaPtrP", {
  __tostring = function (self) return "P(" .. tostring(self.n) .. ")" end,
  __len = function (self) return self.n end,
  __pairs = function (self)
    local key = 0
    return function ()
      key = key + 1
      if key <= self.n then return key, key * 10 end
    end
  end,
})

local p = ffi.new("LuarMetaPtrP", 3)
local pp = ffi.cast("LuarMetaPtrP *", p)

H.eq(tostring(p), "P(3)")
H.eq(tostring(pp), "P(3)", "__tostring was not forwarded to a pointer")
H.eq(#p, 3)
H.eq(#pp, 3, "__len was not forwarded to a pointer")

local function collect (x)
  local out = {}
  for k, v in pairs(x) do out[#out + 1] = k .. "=" .. v end
  return table.concat(out, ",")
end
H.eq(collect(p), "1=10,2=20,3=30")
H.eq(collect(pp), "1=10,2=20,3=30", "__pairs was not forwarded to a pointer")

-- A metatype without a handler keeps the clear fallback errors.
local Q = ffi.metatype("LuarMetaPtrQ", {})
local q = ffi.new("LuarMetaPtrQ", 1)
local qp = ffi.cast("LuarMetaPtrQ *", q)
H.raises(function () return #q end, "length")
H.raises(function () return #qp end, "length")
H.raises(function () for _ in pairs(q) do end end, "iterate")
H.raises(function () for _ in pairs(qp) do end end, "iterate")

-- Ordinary pointer formatting and scalar cdata are unchanged.
local ints = ffi.new("int[2]")
local ip = ffi.cast("int *", ints)
H.check(tostring(ip):find("cdata<int %*>", 1) ~= nil, tostring(ip))
H.raises(function () for _ in pairs(ffi.new("int")) do end end, "iterate")

print("ffi/116_metatype_pointer ok")
