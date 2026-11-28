-- ffi/280_j2_int_sign_extension_update.lua -- J2 narrow signed cdata updates
-- must sign-extend before wide integer arithmetic.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/280_j2_int_sign_extension_update: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int64_t i; int32_t z; } NF280;
]]

do
  local s = ffi.new("NF280"); s.i, s.z = 1, 4
  local V = ffi.new("int32_t", -3)
  local function fill (o, v, n) for _ = 1, n do o.i = o.i + v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "int64 add int32 did not run")
  H.eq(s.i, -599, "int64 add int32")
  H.eq(s.z, 4, "int32 sibling")
end

do
  local s = ffi.new("NF280"); s.i, s.z = 1, 4
  local V = ffi.new("int16_t", -3)
  local function fill (o, v, n) for _ = 1, n do o.i = o.i - v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "int64 sub int16 did not run")
  H.eq(s.i, 601, "int64 sub int16")
  H.eq(s.z, 4, "int16 sibling")
end

do
  local s = ffi.new("NF280"); s.i, s.z = 1, 4
  local V = ffi.new("int8_t", -2)
  local function fill (o, v, n) for _ = 1, n do o.i = o.i + v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "int64 add int8 did not run")
  H.eq(s.i, -399, "int64 add int8")
  H.eq(s.z, 4, "int8 sibling")
end

do
  local U = ffi.new("NF280"); U.i, U.z = 1, 4
  local V = ffi.new("int16_t", -3)
  local function fill (n) for _ = 1, n do U.i = U.i + V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.eq(U.i, -599, "upvalue value")
  H.eq(U.z, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/280_j2_int_sign_extension_update ok")
