-- ffi/207_j2_f32_array_fill.lua -- J2 float32 array fills.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/207_j2_f32_array_fill: skipped (no native target)")
  return
end

local N = 128
local function init () return ffi.new("float[?]", N) end

local function check (name, a, val)
  for i = 0, N - 1 do H.eq(a[i], val, name .. " " .. i) end
end

local function run (name, f)
  local a = init()
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(a, N)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  return a
end

check("const", run("const", function (a, n)
  for i = 0, n - 1 do a[i] = 2.5 end
end), 2.5)

check("negative", run("negative", function (a, n)
  for i = 0, n - 1 do a[i] = -3.25 end
end), -3.25)

check("integer", run("integer", function (a, n)
  for i = 0, n - 1 do a[i] = 7 end
end), 7)

do
  local v = 3.5
  check("number", run("number", function (a, n)
    for i = 0, n - 1 do a[i] = v end
  end), 3.5)
end

do
  local v = ffi.new("float", 4.5)
  check("cdata", run("cdata", function (a, n)
    for i = 0, n - 1 do a[i] = v end
  end), 4.5)
end

check("zero", run("zero", function (a, n)
  for i = 0, n - 1 do a[i] = 0 end
end), 0)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/207_j2_f32_array_fill ok")
