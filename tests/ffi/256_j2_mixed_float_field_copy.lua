-- ffi/256_j2_mixed_float_field_copy.lua -- J2 float/double field conversion.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/256_j2_mixed_float_field_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { float f; double d; int64_t z; } NF256;
]]

local function check (name, s, f, d)
  H.eq(s.f, f, name .. ": f")
  H.eq(s.d, d, name .. ": d")
  H.eq(s.z, 4, name .. ": sibling")
end

do
  local s = ffi.new("NF256"); s.f, s.d, s.z = 2.5, 0, 4
  local function copy (o, n) for _ = 1, n do o.d = o.f end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(s, 200)
  H.check(jit.stats().tint_hits > before, "float->double did not run")
  check("float->double", s, 2.5, 2.5)
end

do
  local s = ffi.new("NF256"); s.f, s.d, s.z = 0, -3.25, 4
  local function copy (o, n) for _ = 1, n do o.f = o.d end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(s, 200)
  H.check(jit.stats().tint_hits > before, "double->float did not run")
  check("double->float", s, -3.25, -3.25)
end

do
  local S = ffi.new("NF256"); S.f, S.d, S.z = 9.5, 0, 4
  local D = ffi.new("NF256"); D.f, D.d, D.z = 0, 0, 4
  local function copy (a, b, n) for _ = 1, n do b.d = a.f end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(S, D, 200)
  H.check(jit.stats().tint_hits > before, "cross did not run")
  check("cross", D, 0, 9.5)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/256_j2_mixed_float_field_copy ok")
