-- ffi/260_j2_int_cdata_float_field_update.lua -- J2 float/double field
-- updates from cdata integer scalars.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/260_j2_int_cdata_float_field_update: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { double d; float f; int64_t z; } NF260;
]]

local function run (name, f, get)
  local w = ffi.new("NF260"); w.d, w.f, w.z = 1, 1, 4
  local V = ffi.new("int32_t", -7)
  jit.off(); f(w, V, 200)
  local want = get(w)

  local s = ffi.new("NF260"); s.d, s.f, s.z = 1, 1, 4
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(s, V, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), want, name .. ": value")
  H.eq(s.z, 4, name .. ": sibling")
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

do
  local U = ffi.new("NF260"); U.d, U.f, U.z = 1, 1, 4
  local V = ffi.new("int32_t", -7)
  local function fill (n) for _ = 1, n do U.d = U.d + V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.eq(U.d, -1399.0, "upvalue value")
  H.eq(U.z, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/260_j2_int_cdata_float_field_update ok")
