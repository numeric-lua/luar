-- ffi/208_j2_upvalue_f64_fill.lua -- J2 float/double fills through an
-- upvalue array destination.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/208_j2_upvalue_f64_fill: skipped (no native target)")
  return
end

local N = 128
local function check (name, a, val)
  for i = 0, N - 1 do H.eq(a[i], val, name .. " " .. i) end
end

-- Register cdata scalar into an upvalue double array.
do
  local A = ffi.new("double[?]", N)
  local V = ffi.new("double", 2.5)
  local function fill (v, n) for i = 0, n - 1 do A[i] = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(V, N)
  H.check(jit.stats().tint_hits > before, "upvalue register fill did not run")
  check("register", A, 2.5)
end

-- Upvalue scalar into an upvalue double array.
do
  local A = ffi.new("double[?]", N)
  local V = ffi.new("double", -3.25)
  local function fill (n) for i = 0, n - 1 do A[i] = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(N)
  H.check(jit.stats().tint_hits > before, "upvalue scalar fill did not run")
  check("upvalue", A, -3.25)
end

-- float32 upvalue array.
do
  local A = ffi.new("float[?]", N)
  local V = ffi.new("float", 4.5)
  local function fill (n) for i = 0, n - 1 do A[i] = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(N)
  H.check(jit.stats().tint_hits > before, "float32 upvalue fill did not run")
  check("float32", A, 4.5)
end

-- Zero fill.
do
  local A = ffi.new("double[?]", N)
  local V = ffi.new("double", 0)
  local function fill (n) for i = 0, n - 1 do A[i] = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(N)
  H.check(jit.stats().tint_hits > before, "zero upvalue fill did not run")
  check("zero", A, 0)
end

-- Constant fills through an upvalue array.
do
  local A = ffi.new("double[?]", N)
  local function run (name, val, f)
    jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
    local before = jit.stats().tint_hits
    f(N)
    H.check(jit.stats().tint_hits > before, name .. " constant fill did not run")
    check(name, A, val)
  end
  run("const float", 2.5, function (n)
    for i = 0, n - 1 do A[i] = 2.5 end
  end)
  run("const negative", -3.25, function (n)
    for i = 0, n - 1 do A[i] = -3.25 end
  end)
  run("const integer", 7.0, function (n)
    for i = 0, n - 1 do A[i] = 7 end
  end)
  run("const zero", 0.0, function (n)
    for i = 0, n - 1 do A[i] = 0 end
  end)
end

-- float32 constant through an upvalue array.
do
  local A = ffi.new("float[?]", N)
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  local function fill (n) for i = 0, n - 1 do A[i] = 4.5 end end
  fill(N)
  H.check(jit.stats().tint_hits > before, "float32 constant fill did not run")
  check("float32 const", A, 4.5)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/208_j2_upvalue_f64_fill ok")
