-- ffi/294_j2_int_field_double_fill_wide.lua -- J2 integer field fills from
-- floating cdata beyond the signed 64-bit range.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/294_j2_int_field_double_fill_wide: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { uint64_t u; int64_t i; int32_t j; int32_t z; } NF294;
]]

do
  local s = ffi.new("NF294"); s.u, s.i, s.j, s.z = 0, 0, 0, 4
  local V = ffi.new("double", 1e19)
  local function fill (o, v, n) for _ = 1, n do o.u = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "wide uint64 did not run")
  H.check(s.u == ffi.new("uint64_t", 10000000000000000000),
          "wide uint64")
  H.eq(s.z, 4, "wide sibling")
end

do
  local s = ffi.new("NF294"); s.u, s.i, s.j, s.z = 0, 0, 0, 4
  local V = ffi.new("double", 42.9)
  local function fill (n) for _ = 1, n do s.u = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue uint64 did not run")
  H.check(s.u == ffi.new("uint64_t", 42), "upvalue uint64")
  H.eq(s.z, 4, "upvalue sibling")
end

do
  local s = ffi.new("NF294"); s.u, s.i, s.j, s.z = 0, 0, 0, 4
  local V = ffi.new("double", -3.9)
  local function fill (o, v, n) for _ = 1, n do o.i = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "int64 did not run")
  H.check(s.i == ffi.new("int64_t", -3), "int64")
  H.eq(s.z, 4, "int64 sibling")
end

do
  local s = ffi.new("NF294"); s.u, s.i, s.j, s.z = 0, 0, 0, 4
  local V = ffi.new("float", 7.9)
  local function fill (o, v, n) for _ = 1, n do o.j = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "int32 did not run")
  H.eq(s.j, 7, "int32")
  H.eq(s.z, 4, "int32 sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/294_j2_int_field_double_fill_wide ok")
