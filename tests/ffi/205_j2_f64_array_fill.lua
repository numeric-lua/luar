-- ffi/205_j2_f64_array_fill.lua -- J2 double-array fills from cdata.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/205_j2_f64_array_fill: skipped (no native target)")
  return
end

local N = 64
local function init ()
  local a = ffi.new("double[?]", N)
  for i = 0, N - 1 do a[i] = i + 0.5 end
  return a
end

local function check_range (name, a, val, step)
  step = step or 1
  for i = 0, N - 1 do
    if (i % step) == 0 then
      if val ~= val then
        H.check(a[i] ~= a[i], name .. " nan " .. i)
      else
        H.eq(a[i], val, name .. " " .. i)
      end
    else
      H.eq(a[i], i + 0.5, name .. " untouched " .. i)
    end
  end
end

-- Register scalar.
do
  local a = init()
  local V = ffi.new("double", 2.5)
  local function fill (x, v, n) for i = 0, n - 1 do x[i] = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(a, V, N)
  H.check(jit.stats().tint_hits > before, "register fill did not run")
  check_range("register", a, 2.5)
end

-- Upvalue scalar.
do
  local a = init()
  local V = ffi.new("double", -3.25)
  local function fill (x, n) for i = 0, n - 1 do x[i] = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(a, N)
  H.check(jit.stats().tint_hits > before, "upvalue fill did not run")
  check_range("upvalue", a, -3.25)
end

-- NaN scalar.
do
  local a = init()
  local V = ffi.new("double", 0/0)
  local function fill (x, v, n) for i = 0, n - 1 do x[i] = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(a, V, N)
  H.check(jit.stats().tint_hits > before, "nan fill did not run")
  check_range("nan", a, 0/0)
end

-- +Inf scalar.
do
  local a = init()
  local V = ffi.new("double", 1/0)
  local function fill (x, v, n) for i = 0, n - 1 do x[i] = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(a, V, N)
  H.check(jit.stats().tint_hits > before, "inf fill did not run")
  check_range("inf", a, 1/0)
end

-- Numeric constant fills.
do
  local function run (name, val, f)
    local a = init()
    jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
    local before = jit.stats().tint_hits
    f(a, N)
    H.check(jit.stats().tint_hits > before, name .. " constant fill did not run")
    check_range(name, a, val)
  end
  run("const float", 2.5, function (a, n)
    for i = 0, n - 1 do a[i] = 2.5 end
  end)
  run("const negative", -3.25, function (a, n)
    for i = 0, n - 1 do a[i] = -3.25 end
  end)
  run("const integer", 7.0, function (a, n)
    for i = 0, n - 1 do a[i] = 7 end
  end)
  run("const zero", 0.0, function (a, n)
    for i = 0, n - 1 do a[i] = 0 end
  end)
  run("const large", 1e300, function (a, n)
    for i = 0, n - 1 do a[i] = 1e300 end
  end)
end

-- Lua-number scalar fills (register and upvalue).
do
  local function runnum (name, val, f)
    local a = init()
    jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
    local before = jit.stats().tint_hits
    f(a, N)
    H.check(jit.stats().tint_hits > before, name .. " number fill did not run")
    check_range(name, a, val)
  end
  runnum("num float", 2.5, function (a, n)
    local v = 2.5
    for i = 0, n - 1 do a[i] = v end
  end)
  runnum("num negative", -3.25, function (a, n)
    local v = -3.25
    for i = 0, n - 1 do a[i] = v end
  end)
  runnum("num integer", 7.0, function (a, n)
    local v = 7
    for i = 0, n - 1 do a[i] = v end
  end)
  runnum("num zero", 0.0, function (a, n)
    local v = 0
    for i = 0, n - 1 do a[i] = v end
  end)
  runnum("num large", 1e300, function (a, n)
    local v = 1e300
    for i = 0, n - 1 do a[i] = v end
  end)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/205_j2_f64_array_fill ok")
