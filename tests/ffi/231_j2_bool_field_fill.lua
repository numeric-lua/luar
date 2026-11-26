-- ffi/231_j2_bool_field_fill.lua -- J2 _Bool field fills from numbers.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/231_j2_bool_field_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { _Bool b; int64_t d; } NBF;
]]

local function run (name, value, want)
  local s = ffi.new("NBF")
  s.b, s.d = false, 4
  local function fill (o, v, n) for _ = 1, n do o.b = v end end
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  fill(s, value, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(s.b, want, name .. ": value")
  H.eq(s.d, 4, name .. ": sibling")
end

run("true", 1, true)
run("false", 0, false)
run("nonzero", 2, true)
run("negative", -1, true)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/231_j2_bool_field_fill ok")
