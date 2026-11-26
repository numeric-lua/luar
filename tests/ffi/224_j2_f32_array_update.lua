-- ffi/224_j2_f32_array_update.lua -- J2 float32 array updates.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/224_j2_f32_array_update: skipped (no native target)")
  return
end

local N = 128
local function init ()
  local a = ffi.new("float[?]", N)
  for i = 0, N - 1 do a[i] = i + 0.5 end
  return a
end

local function run (name, f)
  local want = init()
  jit.off()
  f(want, N)
  local a = init()
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(a, N)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  for i = 0, N - 1 do H.eq(a[i], want[i], name .. " " .. i) end
end

local function add (a, n) for i = 0, n - 1 do a[i] = a[i] + 1.5 end end
local function sub (a, n) for i = 0, n - 1 do a[i] = a[i] - 1.5 end end
local function mul (a, n) for i = 0, n - 1 do a[i] = a[i] * 2.0 end end
local function div (a, n) for i = 0, n - 1 do a[i] = a[i] / 2.0 end end
local function mod (a, n) for i = 0, n - 1 do a[i] = a[i] % 2.0 end end

run("add", add)
run("subtract", sub)
run("multiply", mul)
run("divide", div)
run("modulo", mod)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/224_j2_f32_array_update ok")
