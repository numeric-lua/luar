-- ffi/284_j2_uint64_float_copy.lua -- J2 mixed uint64/float field copies.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/284_j2_uint64_float_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { uint64_t u; double d; float f; int32_t z; } NF284;
]]

do
  local s = ffi.new("NF284"); s.u, s.d, s.f, s.z = -1, 0, 0, 4
  local function copy (o, n) for _ = 1, n do o.d = o.u end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(s, 200)
  H.check(jit.stats().tint_hits > before, "uint64->double did not run")
  H.eq(s.d, 18446744073709551616.0, "uint64->double")
  H.eq(s.z, 4, "double sibling")
end

do
  local s = ffi.new("NF284"); s.u, s.d, s.f, s.z = 0x8000000000000000, 0, 0, 4
  local function copy (o, n) for _ = 1, n do o.f = o.u end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(s, 200)
  H.check(jit.stats().tint_hits > before, "uint64->float did not run")
  H.eq(s.f, 9223372036854775808.0, "uint64->float")
  H.eq(s.z, 4, "float sibling")
end

do
  local U = ffi.new("NF284"); U.u, U.d, U.f, U.z = 42, 0, 0, 4
  local D = ffi.new("NF284"); D.u, D.d, D.f, D.z = 0, 0, 0, 4
  local function copy (o, n) for _ = 1, n do o.d = U.u end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.eq(D.d, 42.0, "upvalue")
  H.eq(D.z, 4, "upvalue sibling")
end

do
  local U = ffi.new("NF284"); U.u, U.d, U.f, U.z = 7, 0, 0, 4
  local D = ffi.new("NF284"); D.u, D.d, D.f, D.z = 0, 0, 0, 4
  local function copy (o, n) for _ = 1, n do o.f = U.u end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "upvalue float did not run")
  H.eq(D.f, 7.0, "upvalue float")
  H.eq(D.z, 4, "upvalue float sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/284_j2_uint64_float_copy ok")
