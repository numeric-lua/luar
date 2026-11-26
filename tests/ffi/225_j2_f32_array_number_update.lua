-- ffi/225_j2_f32_array_number_update.lua -- J2 float32 updates from Lua
-- number scalars.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/225_j2_f32_array_number_update: skipped (no native target)")
  return
end

local N = 128
local function init ()
  local a = ffi.new("float[?]", N)
  for i = 0, N - 1 do a[i] = i + 0.5 end
  return a
end

local function run (name, f, v)
  local want = init()
  jit.off()
  f(want, v, N)
  local a = init()
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(a, v, N)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  for i = 0, N - 1 do H.eq(a[i], want[i], name .. " " .. i) end
end

local function add (a, v, n) for i = 0, n - 1 do a[i] = a[i] + v end end
local function sub (a, v, n) for i = 0, n - 1 do a[i] = a[i] - v end end
local function mul (a, v, n) for i = 0, n - 1 do a[i] = a[i] * v end end
local function div (a, v, n) for i = 0, n - 1 do a[i] = a[i] / v end end
local function mod (a, v, n) for i = 0, n - 1 do a[i] = a[i] % v end end

run("add", add, 1.5)
run("subtract", sub, 1.5)
run("multiply", mul, 2.0)
run("divide", div, 2.0)
run("modulo", mod, 2.0)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/225_j2_f32_array_number_update ok")
