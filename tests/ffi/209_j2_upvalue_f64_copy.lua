-- ffi/209_j2_upvalue_f64_copy.lua -- J2 copies into an upvalue float array.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/209_j2_upvalue_f64_copy: skipped (no native target)")
  return
end

local N = 128
local function init (ctype, off)
  local a = ffi.new(ctype, N)
  for i = 0, N - 1 do a[i] = i * 0.5 + (off or 0) end
  return a
end

local function run (name, U, src, f, check)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(src, N)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  for i = 0, N - 1 do check(i, U[i]) end
end

do
  local U = init("double[?]", -7)
  local S = init("double[?]", 1.25)
  local function copy (s, n) for i = 0, n - 1 do U[i] = s[i] end end
  run("double", U, S, copy, function (i, v)
    H.eq(v, i * 0.5 + 1.25, "double " .. i)
  end)
end

do
  local U = init("float[?]", -7)
  local S = init("float[?]", 1.25)
  local function copy (s, n) for i = 0, n - 1 do U[i] = s[i] end end
  run("float", U, S, copy, function (i, v)
    H.eq(v, i * 0.5 + 1.25, "float " .. i)
  end)
end

do
  local U = init("float[?]", -7)
  local S = init("double[?]", 1.25)
  local function copy (s, n) for i = 0, n - 1 do U[i] = s[i] end end
  run("double->float", U, S, copy, function (i, v)
    H.eq(v, i * 0.5 + 1.25, "cross " .. i)
  end)
end

do
  local U = init("double[?]", -7)
  local S = init("double[?]", 0)
  local function copy (s, n) for i = 0, n - 1, 2 do U[i] = s[i] end end
  run("strided", U, S, copy, function (i, v)
    if (i % 2) == 0 then
      H.eq(v, i * 0.5, "strided " .. i)
    else
      H.eq(v, i * 0.5 - 7, "strided untouched " .. i)
    end
  end)
end

do
  local U = init("double[?]", 0)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local function copy (s, n) for i = 0, n - 1 do U[i] = s[i] end end
  copy(U, N)
  H.check(jit.stats().tint_hits > before, "self copy did not run")
  for i = 0, N - 1 do H.eq(U[i], i * 0.5, "self " .. i) end
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/209_j2_upvalue_f64_copy ok")
