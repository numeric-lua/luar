-- ffi/199_j2_i64_field_sum.lua -- J2 reductions over 8-byte struct fields
-- into a cdata accumulator.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/199_j2_i64_field_sum: skipped (no native target)")
  return
end

ffi.cdef[[ typedef struct { int64_t x; int64_t y; } FS; ]]

local function run (name, f, init)
  local w = ffi.new("FS")
  w.x, w.y = 7, 9
  jit.off()
  local want = f(w, 200)

  local s = ffi.new("FS")
  s.x, s.y = 7, 9
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(got, want, name .. ": accumulator")
  H.eq(type(got), "cdata", name .. ": cdata accumulator")
  H.eq(s.x, 7, name .. ": field unchanged")
  H.eq(s.y, 9, name .. ": sibling unchanged")
end

local function add (o, n)
  local a = ffi.new("int64_t", 0)
  for _ = 1, n do a = a + o.x end
  return a
end
local function sub (o, n)
  local a = ffi.new("int64_t", 100000)
  for _ = 1, n do a = a - o.x end
  return a
end
local function mul (o, n)
  local a = ffi.new("int64_t", 1)
  for _ = 1, n do a = a * o.x end
  return a
end
local function band (o, n)
  local a = ffi.new("int64_t", -1)
  for _ = 1, n do a = a & o.x end
  return a
end
local function shl (o, n)
  local a = ffi.new("int64_t", 1)
  for _ = 1, n do a = a << o.x end
  return a
end

run("add", add)
run("subtract", sub)
run("multiply", mul)
run("and", band)
run("shift", shl)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/199_j2_i64_field_sum ok")
