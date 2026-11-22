-- ffi/105_scalar_call_cache.lua -- scalar libffi call interfaces are
-- cached per CType; repeated calls must keep exact types and values.
local H = dofile("tests/helpers.lua")

local ffi = require("ffi")
if not luar.features.ffi then
  print("ffi/105_scalar_call_cache: skipped (FFI not built)")
  return
end

ffi.cdef[[
  int abs(int);
  double fabs(double);
  double hypot(double, double);
  double atan2(double, double);
]]

local abs, fabs, hypot, atan2 = ffi.C.abs, ffi.C.fabs,
                                ffi.C.hypot, ffi.C.atan2
H.eq(type(abs), "cdata")

for i = -1000, 1000 do
  H.eq(abs(i), math.abs(i))
  H.near(fabs(i + 0.5), math.abs(i + 0.5), 1e-12)
  H.near(hypot(i, 1), math.sqrt(i * i + 1), 1e-9)
end
H.near(atan2(1, 1), math.pi / 4, 1e-15)
H.eq(type(fabs(2.5)), "number")
H.eq(tonumber(abs(-3)), 3)

-- Exercise the same signatures from J4, where the cached interfaces are on
-- the trace hot path.
local function sumabs (n)
  local s = 0
  for i = -n, n do s = s + abs(i) end
  return s
end
local function sumhypot (n)
  local s = 0.0
  for i = 1, n do s = s + hypot(i, 1.0) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local want_hypot = 0.0
for i = 1, 2000 do want_hypot = want_hypot + math.sqrt(i * i + 1) end
H.eq(sumabs(2000), 2000 * 2001)
H.near(sumhypot(2000), want_hypot, 1e-6)
jit.flush()
jit.opt.start("ir=0")
jit.on()

print("ffi/105_scalar_call_cache ok")
