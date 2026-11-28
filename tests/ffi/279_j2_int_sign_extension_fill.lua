-- ffi/279_j2_int_sign_extension_fill.lua -- J2 narrow signed integer fills
-- must sign-extend into wider integer fields.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/279_j2_int_sign_extension_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int64_t i; int32_t z; } NF279;
]]

do
  local s = ffi.new("NF279"); s.i, s.z = 0, 4
  local V = ffi.new("int32_t", -3)
  local function fill (o, v, n) for _ = 1, n do o.i = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "int32->int64 did not run")
  H.eq(s.i, -3, "int32->int64")
  H.eq(s.z, 4, "int32 sibling")
end

do
  local s = ffi.new("NF279"); s.i, s.z = 0, 4
  local V = ffi.new("int16_t", -3)
  local function fill (o, v, n) for _ = 1, n do o.i = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "int16->int64 did not run")
  H.eq(s.i, -3, "int16->int64")
  H.eq(s.z, 4, "int16 sibling")
end

do
  local s = ffi.new("NF279"); s.i, s.z = 0, 4
  local V = ffi.new("uint32_t", 0xffffffff)
  local function fill (o, v, n) for _ = 1, n do o.i = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "uint32->int64 did not run")
  H.eq(s.i, 4294967295, "uint32->int64")
  H.eq(s.z, 4, "uint32 sibling")
end

do
  local s = ffi.new("NF279"); s.i, s.z = 0, 4
  local V = ffi.new("int32_t", -7)
  local function fill (n) for _ = 1, n do s.i = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.eq(s.i, -7, "upvalue")
  H.eq(s.z, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/279_j2_int_sign_extension_fill ok")
