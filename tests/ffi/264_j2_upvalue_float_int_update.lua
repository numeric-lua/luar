-- ffi/264_j2_upvalue_float_int_update.lua -- J2 upvalue float/double field
-- updates from cdata integer scalars.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/264_j2_upvalue_float_int_update: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { double d; float f; int64_t z; } NF264;
]]

local function run (name, f, get)
  local w = ffi.new("NF264"); w.d, w.f, w.z = 1, 1, 4
  local V = ffi.new("int32_t", -7)
  jit.off(); f(w, V, 200)
  local want = get(w)

  local U = ffi.new("NF264"); U.d, U.f, U.z = 1, 1, 4
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  f(U, V, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(U), want, name .. ": value")
  H.eq(U.z, 4, name .. ": sibling")
end

run("double add", function (o, v, n)
  for _ = 1, n do o.d = o.d + v end
end, function (s) return s.d end)

run("double sub", function (o, v, n)
  for _ = 1, n do o.d = o.d - v end
end, function (s) return s.d end)

run("float add", function (o, v, n)
  for _ = 1, n do o.f = o.f + v end
end, function (s) return s.f end)

run("float sub", function (o, v, n)
  for _ = 1, n do o.f = o.f - v end
end, function (s) return s.f end)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/264_j2_upvalue_float_int_update ok")
