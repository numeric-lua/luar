-- ffi/300_j2_double_bool_field_fill.lua -- J2 bool field fills from double
-- cdata normalize truth values.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/300_j2_double_bool_field_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { _Bool b; int32_t z; } NF300;
]]

local function run (name, value, want)
  local s = ffi.new("NF300"); s.b, s.z = false, 4
  local V = ffi.new("double", value)
  local function fill (o, v, n) for _ = 1, n do o.b = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(s.b, want, name .. ": value")
  H.eq(s.z, 4, name .. ": sibling")
end

run("0.0", 0.0, false)
run("2.5", 2.5, true)
run("-1.0", -1.0, true)
run("-0.0", -0.0, false)

do
  local s = ffi.new("NF300"); s.b, s.z = false, 4
  local V = ffi.new("double", 0.0)
  local function fill (n) for _ = 1, n do s.b = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.eq(s.b, false, "upvalue value")
  H.eq(s.z, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/300_j2_double_bool_field_fill ok")
