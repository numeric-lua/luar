-- ffi/303_j2_int64_uint64_field_fill.lua -- J2 uint64 fills from int64 cdata
-- preserve raw two's-complement bits.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/303_j2_int64_uint64_field_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { uint64_t u; int32_t z; } NF303;
]]

local function run (name, value, want)
  local s = ffi.new("NF303"); s.u, s.z = 0, 4
  local V = ffi.new("int64_t", value)
  local function fill (o, v, n) for _ = 1, n do o.u = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.check(s.u == ffi.new("uint64_t", want), name .. ": value")
  H.eq(s.z, 4, name .. ": sibling")
end

run("1", 1, 1)
run("INT64_MAX", 0x7fffffffffffffff, 0x7fffffffffffffff)
run("INT64_MIN", -0x7fffffffffffffff - 1, -0x7fffffffffffffff - 1)
run("-1", -1, -1)

do
  local s = ffi.new("NF303"); s.u, s.z = 0, 4
  local V = ffi.new("int64_t", -9)
  local function fill (n) for _ = 1, n do s.u = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.check(s.u == ffi.new("uint64_t", -9), "upvalue value")
  H.eq(s.z, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/303_j2_int64_uint64_field_fill ok")
