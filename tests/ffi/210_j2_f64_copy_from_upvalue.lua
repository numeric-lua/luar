-- ffi/210_j2_f64_copy_from_upvalue.lua -- J2 float-array copies from an
-- upvalue source.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/210_j2_f64_copy_from_upvalue: skipped (no native target)")
  return
end

local N = 128
local function init (ctype, off)
  local a = ffi.new(ctype, N)
  for i = 0, N - 1 do a[i] = i * 0.5 + (off or 0) end
  return a
end

local function run (name, D, f, check)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(D, N)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  for i = 0, N - 1 do check(i, D[i]) end
end

do
  local U = init("double[?]", 1.25)
  local function copy (d, n) for i = 0, n - 1 do d[i] = U[i] end end
  local D = init("double[?]", -7)
  run("double", D, copy, function (i, v)
    H.eq(v, i * 0.5 + 1.25, "double " .. i)
  end)
end

do
  local U = init("float[?]", 1.25)
  local function copy (d, n) for i = 0, n - 1 do d[i] = U[i] end end
  local D = init("float[?]", -7)
  run("float", D, copy, function (i, v)
    H.eq(v, i * 0.5 + 1.25, "float " .. i)
  end)
end

do
  local U = init("double[?]", 1.25)
  local function copy (d, n) for i = 0, n - 1 do d[i] = U[i] end end
  local D = init("float[?]", -7)
  run("double->float", D, copy, function (i, v)
    H.eq(v, i * 0.5 + 1.25, "cross " .. i)
  end)
end

do
  local U = init("double[?]", 0)
  local D = init("double[?]", -7)
  local function copy (d, n) for i = 0, n - 1, 2 do d[i] = U[i] end end
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  copy(D, N)
  H.check(jit.stats().tint_hits > before, "strided copy did not run")
  for i = 0, N - 1 do
    if (i % 2) == 0 then H.eq(D[i], i * 0.5, "strided " .. i)
    else H.eq(D[i], i * 0.5 - 7, "strided untouched " .. i) end
  end
end

do
  local U = init("double[?]", 1.25)
  local D = init("double[?]", -7)
  local function copy (n) for i = 0, n - 1 do D[i] = U[i] end end
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  copy(N)
  H.check(jit.stats().tint_hits > before, "both-upvalue copy did not run")
  for i = 0, N - 1 do H.eq(D[i], i * 0.5 + 1.25, "both " .. i) end
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/210_j2_f64_copy_from_upvalue ok")
