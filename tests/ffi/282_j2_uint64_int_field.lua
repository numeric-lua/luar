-- ffi/282_j2_uint64_int_field.lua -- J2 unsigned 64-bit cdata fills/updates
-- into int64 fields preserve raw bits.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/282_j2_uint64_int_field: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int64_t i; int32_t z; } NF282;
]]

do
  local s = ffi.new("NF282"); s.i, s.z = 0, 4
  local V = ffi.new("uint64_t", -1)
  local function fill (o, v, n) for _ = 1, n do o.i = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "fill did not run")
  H.eq(s.i, -1, "fill")
  H.eq(s.z, 4, "fill sibling")
end

do
  local s = ffi.new("NF282"); s.i, s.z = 0, 4
  local V = ffi.new("uint64_t", 0x8000000000000000)
  local function fill (o, v, n) for _ = 1, n do o.i = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "high bit fill did not run")
  H.eq(s.i, -9223372036854775808, "high bit fill")
  H.eq(s.z, 4, "high bit sibling")
end

do
  local s = ffi.new("NF282"); s.i, s.z = 1, 4
  local V = ffi.new("uint64_t", 2)
  local function fill (o, v, n) for _ = 1, n do o.i = o.i + v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "add did not run")
  H.eq(s.i, 401, "add")
  H.eq(s.z, 4, "add sibling")
end

do
  local s = ffi.new("NF282"); s.i, s.z = 0, 4
  local V = ffi.new("uint64_t", -1)
  local function fill (n) for _ = 1, n do s.i = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.eq(s.i, -1, "upvalue")
  H.eq(s.z, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/282_j2_uint64_int_field ok")
