-- ffi/192_j2_i64_fill.lua -- J2 fill of 8-byte integer arrays from a
-- cdata int64/uint64 value.
--
-- One raw 8-byte value is stored repeatedly; register and upvalue
-- destinations, strided/descending runs, mixed signedness and
-- out-of-bounds fallback are covered.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/192_j2_i64_fill: skipped (no native target)")
  return
end

local N = 200
local VI = ffi.new("int64_t", -1234567890123)
local VU = ffi.new("uint64_t", 1234567890123)

local function run (name, f, val, want)
  local d = ffi.new("int64_t[?]", N)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(d, val, N)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  for i = 0, N - 1 do H.eq(d[i], want(i), name .. ": element " .. i) end
end

local function fill (d, v, n) for i = 0, n - 1 do d[i] = v end end
local function fill_stride (d, v, n) for i = 0, n - 1, 3 do d[i] = v end end
local function fill_down (d, v, n) for i = n - 1, 0, -2 do d[i] = v end end

run("int64 value", fill, VI, function () return VI end)
run("uint64 value", fill, VU, function () return VU end)
run("stride", fill_stride, VI, function (i)
  if i % 3 ~= 0 then return ffi.new("int64_t", 0) end
  return VI
end)
run("descending", fill_down, VI, function (i)
  if (N - 1 - i) % 2 ~= 0 then return ffi.new("int64_t", 0) end
  return VI
end)

-- Upvalue destination.
local U = ffi.new("int64_t[?]", N)
local function fill_up (v, n) for i = 0, n - 1 do U[i] = v end end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
fill_up(VI, N)
H.check(jit.stats().tint_hits > before, "upvalue destination did not run")
for i = 0, N - 1 do H.eq(U[i], VI, "upvalue dest " .. i) end

-- Upvalue value.
local UV = ffi.new("int64_t", -777)
local function fill_upval_value (d, n) for i = 0, n - 1 do d[i] = UV end end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
local DV = ffi.new("int64_t[?]", N)
fill_upval_value(DV, N)
H.check(jit.stats().tint_hits > before, "upvalue value fill did not run")
for i = 0, N - 1 do H.eq(DV[i], UV, "upvalue value " .. i) end

-- Out-of-bounds runs fall back and raise.
do
  local function over (d, v, n) for i = 0, n do d[i] = v end end
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  before = jit.stats().tint_hits
  local ok = pcall(function ()
    local d = ffi.new("int64_t[?]", N)
    over(d, VI, N)
  end)
  H.check(not ok, "out-of-bounds i64 fill must raise")
  H.eq(jit.stats().tint_hits, before, "out-of-bounds fill must not use the helper")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/192_j2_i64_fill ok")
