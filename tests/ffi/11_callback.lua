-- ffi/11_callback.lua -- ffi.callback: C closures that call Lua.
local ffi = require "ffi"
local H = dofile("tests/helpers.lua")

ffi.cdef[[
typedef int (*cmp_t)(const void *, const void *);
void qsort(void *base, size_t n, size_t size, cmp_t cmp);
]]

local arr = ffi.new("int[6]", {5, 1, 4, 2, 6, 3})
local calls = 0
local cmp = ffi.callback("int(const void *, const void *)", function (a, b)
  calls = calls + 1
  local x = ffi.cast("const int *", a)[0]
  local y = ffi.cast("const int *", b)[0]
  if x < y then return -1 elseif x > y then return 1 else return 0 end
end)
H.eq(type(cmp), "cdata")

ffi.C.qsort(arr, 6, ffi.sizeof("int"), cmp)
for i = 0, 5 do H.eq(arr[i], i + 1) end
H.check(calls > 0, "comparator was not called")

-- a callback cdata is itself callable
H.eq(cmp(ffi.cast("const int *", arr), ffi.cast("const int *", arr)), 0)

-- the typedef spelling works too
local cmp2 = ffi.callback("cmp_t", function (a, b) return 0 end)
H.eq(cmp2(ffi.cast("const int *", arr), ffi.cast("const int *", arr)), 0)

-- Lua errors inside a callback become a zero return and a stored message
local bad = ffi.callback("int(void)", function () error("boom") end)
H.eq(bad(), 0)
local msg = ffi.callback_error()
H.check(type(msg) == "string" and msg:find("boom", 1, true), tostring(msg))
H.eq(ffi.callback_error(), nil)          -- cleared after reading

-- callbacks survive GC when reachable, and unreachable ones are freed
local keep = ffi.callback("int(int)", function (x) return x * 2 end)
for i = 1, 500 do
  ffi.callback("int(int)", function (x) return x + i end)
end
collectgarbage()
collectgarbage()
H.eq(keep(21), 42)
H.eq(ffi.callback_error(), nil)

-- ffi.gc finalizers still run for callback cdata
local finalized = false
local doomed = ffi.callback("int(void)", function () return 1 end)
ffi.gc(doomed, function () finalized = true end)
doomed = nil
collectgarbage()
collectgarbage()
H.eq(finalized, true)

-- unsupported callback shapes fail clearly
H.raises(function ()
  return ffi.callback("struct { int a; }(void)", function () end)
end, "aggregate")
H.raises(function ()
  return ffi.callback("int(int, ...)", function () return 0 end)
end, "variadic")

print("ffi/11_callback ok")
