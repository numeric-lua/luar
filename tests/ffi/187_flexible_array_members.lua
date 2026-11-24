-- ffi/187_flexible_array_members.lua -- C99 flexible array members `T[]`.
--
-- An empty-bracket array is an incomplete type on its own, matching
-- LuaJIT's `sizeof` failure, but as the final struct member it is modeled
-- as a zero-length array so the struct size excludes the payload.  A
-- flexible member anywhere else, or a `[?]` member, is rejected.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

local t = ffi.typeof("int[]")
H.check(tostring(t):find("int[]", 1, true) ~= nil, "standalone flexible type")
H.raises(function () return ffi.sizeof("int[]") end, "variable%-length")
H.raises(function () return ffi.new("int[]") end, "length")

ffi.cdef[[
typedef struct { int n; char data[]; } FlexChar;
typedef struct { int n; double data[]; } FlexDouble;
]]
H.eq(ffi.sizeof("FlexChar"), 4, "char flexible size")
H.eq(ffi.offsetof("FlexChar", "data"), 4, "char flexible offset")
H.eq(ffi.sizeof("FlexDouble"), 8, "double flexible size")
H.eq(ffi.offsetof("FlexDouble", "data"), 8, "double flexible offset")

local f = ffi.new("FlexChar")
f.n = 7
H.eq(f.n, 7, "flexible lead")
H.raises(function () return f.data[0] end, "out of bounds")

H.raises(function ()
  ffi.cdef[[ typedef struct { int n; char data[]; int tail; } FlexBad; ]]
end, "incomplete")
H.raises(function ()
  ffi.cdef[[ typedef struct { int n; char data[?]; } VlaBad; ]]
end, "incomplete")

print("ffi/187_flexible_array_members ok")
