-- ffi/236_j2_bool_upvalue_field_fill.lua -- J2 bool field fills on an
-- upvalue struct.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/236_j2_bool_upvalue_field_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { _Bool b; int64_t d; } NBU;
]]

local function run (name, value, want)
  local U = ffi.new("NBU")
  U.b, U.d = false, 4
  local function fill (u, n) for _ = 1, n do u.b = value end end
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  fill(U, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(U.b, want, name .. ": value")
  H.eq(U.d, 4, name .. ": sibling")
end

run("cdata true", ffi.new("_Bool", true), true)
run("cdata false", ffi.new("_Bool", false), false)
run("number one", 1, true)
run("number zero", 0, false)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/236_j2_bool_upvalue_field_fill ok")
