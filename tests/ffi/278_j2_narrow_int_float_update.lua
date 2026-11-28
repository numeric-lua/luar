-- ffi/278_j2_narrow_int_float_update.lua -- J2 narrow integer field updates
-- from cdata floating scalars.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/278_j2_narrow_int_float_update: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int8_t a; uint8_t b; int16_t c; int64_t z; } NF278;
]]

local function run (name, f, get)
  local w = ffi.new("NF278"); w.a, w.b, w.c, w.z = 1, 1, 1, 4
  local V = ffi.new("double", 2.9)
  jit.off(); f(w, V, 200)
  local want = get(w)

  local s = ffi.new("NF278"); s.a, s.b, s.c, s.z = 1, 1, 1, 4
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  f(s, V, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), want, name .. ": value")
  H.eq(s.z, 4, name .. ": sibling")
end

run("int8 add", function (o, v, n)
  for _ = 1, n do o.a = o.a + v end
end, function (s) return s.a end)

run("uint8 add", function (o, v, n)
  for _ = 1, n do o.b = o.b + v end
end, function (s) return s.b end)

run("int16 add", function (o, v, n)
  for _ = 1, n do o.c = o.c + v end
end, function (s) return s.c end)

do
  local U = ffi.new("NF278"); U.a, U.b, U.c, U.z = 1, 1, 1, 4
  local V = ffi.new("float", 3.1)
  local function fill (n) for _ = 1, n do U.c = U.c + V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.eq(U.c, 601, "upvalue value")
  H.eq(U.z, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/278_j2_narrow_int_float_update ok")
