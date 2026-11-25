-- ffi/213_j2_narrow_field_update.lua -- J2 narrow-field in-place updates.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/213_j2_narrow_field_update: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int32_t a; int16_t b; int8_t c; int64_t d; } NFU;
]]

local function run (name, f, get)
  local w = ffi.new("NFU")
  w.a, w.b, w.c, w.d = 7, 7, 7, 4
  jit.off()
  f(w, 200)
  local want = get(w)

  local s = ffi.new("NFU")
  s.a, s.b, s.c, s.d = 7, 7, 7, 4
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), want, name .. ": value")
  H.eq(s.d, 4, name .. ": int64 sibling")
end

run("int32 add", function (o, n)
  for _ = 1, n do o.a = o.a + 3 end
end, function (s) return s.a end)

run("int16 sub", function (o, n)
  for _ = 1, n do o.b = o.b - 3 end
end, function (s) return s.b end)

run("int8 mul", function (o, n)
  for _ = 1, n do o.c = o.c * 3 end
end, function (s) return s.c end)

run("int32 band", function (o, n)
  for _ = 1, n do o.a = o.a & 0x55 end
end, function (s) return s.a end)

run("int16 shl", function (o, n)
  for _ = 1, n do o.b = o.b << 2 end
end, function (s) return s.b end)

run("int8 shl", function (o, n)
  for _ = 1, n do o.c = o.c << 2 end
end, function (s) return s.c end)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/213_j2_narrow_field_update ok")
