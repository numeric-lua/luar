-- ffi/281_j2_int_wide_copy_sign.lua -- J2 integer field copies preserve signs
-- across widths.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/281_j2_int_wide_copy_sign: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int64_t i; int32_t j; int8_t c; int32_t z; } NF281;
]]

local function run (name, setup, f, get, want)
  local s = ffi.new("NF281"); s.i, s.j, s.c, s.z = 1, 1, 1, 4
  setup(s)
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), want, name .. ": value")
  H.eq(s.z, 4, name .. ": sibling")
end

run("int32->int64", function (s) s.j = -3 end, function (o, n)
  for _ = 1, n do o.i = o.j end
end, function (s) return s.i end, -3)

run("int8->int64", function (s) s.c = -128 end, function (o, n)
  for _ = 1, n do o.i = o.c end
end, function (s) return s.i end, -128)

run("int64->int32", function (s) s.i = -9000000000 end, function (o, n)
  for _ = 1, n do o.j = o.i end
end, function (s) return s.j end, -410065408)

do
  local U = ffi.new("NF281"); U.i, U.j, U.c, U.z = 1, -7, 1, 4
  local D = ffi.new("NF281"); D.i, D.j, D.c, D.z = 1, 1, 1, 4
  local function copy (o, n) for _ = 1, n do o.i = U.j end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  H.eq(D.i, -7, "upvalue value")
  H.eq(D.z, 4, "upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/281_j2_int_wide_copy_sign ok")
