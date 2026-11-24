-- ffi/180_j2_cdata_bitwise_pair.lua -- J2 two-source cdata bitwise stores
-- `a[i] = b[i] OP c[i]`.
--
-- Bitwise and shift operations require integer element arrays.  Negative
-- counts reverse the shift, 64 or more yields zero, and float arrays stay
-- on the interpreter.  In-place and strided destinations are covered.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/180_j2_cdata_bitwise_pair: skipped (no native target)")
  return
end

local function same (a, b)
  return math.type(a) == math.type(b) and a == b
end

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "hotexit=1", "tint=1")
jit.on()

local N = 200
local A = ffi.new("int32_t[?]", N)
local B = ffi.new("int32_t[?]", N)
local U = ffi.new("uint16_t[?]", N)
for i = 0, N - 1 do
  A[i] = i * 37 - 400
  B[i] = (i % 9) + 1
  U[i] = i * 13
end
local NEG = ffi.new("int32_t[?]", N)
local BIG = ffi.new("int32_t[?]", N)
for i = 0, N - 1 do NEG[i] = -3; BIG[i] = 64 end

local function run (name, f, dst, a, b)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local want = f(ffi.new("int32_t[?]", N), a, b, N)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = f(dst or ffi.new("int32_t[?]", N), a, b, N)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  for i = 0, N - 1 do
    H.check(same(got[i], want[i]), name .. ": element " .. i)
    H.eq(math.type(got[i]), math.type(want[i]), name .. ": type " .. i)
  end
  return got
end

local function band (d, a, b, n) for i = 0, n - 1 do d[i] = a[i] & b[i] end return d end
local function bor (d, a, b, n) for i = 0, n - 1 do d[i] = a[i] | b[i] end return d end
local function bxor (d, a, b, n) for i = 0, n - 1 do d[i] = a[i] ~ b[i] end return d end
local function shl (d, a, b, n) for i = 0, n - 1 do d[i] = a[i] << b[i] end return d end
local function shr (d, a, b, n) for i = 0, n - 1 do d[i] = a[i] >> b[i] end return d end
local function band_step (d, a, b, n) for i = 0, n - 1, 3 do d[i] = a[i] & b[i] end return d end

run("band", band, nil, A, B)
run("bor", bor, nil, A, B)
run("bxor", bxor, nil, A, B)
run("shl", shl, nil, A, B)
run("shr", shr, nil, A, B)
run("strided band", band_step, nil, A, B)
run("negative shift", shl, nil, A, NEG)
run("shift by 64", shl, nil, A, BIG)

-- In-place destination is also the first source.
local P = ffi.new("int32_t[?]", N)
for i = 0, N - 1 do P[i] = A[i] end
local function inplace (d, a, b, n) for i = 0, n - 1 do d[i] = d[i] & b[i] end return d end
local W = ffi.new("int32_t[?]", N)
for i = 0, N - 1 do W[i] = A[i] end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.off()
inplace(W, nil, B, N)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
inplace(P, nil, B, N)
H.check(jit.stats().tint_hits > before, "in-place band did not run")
for i = 0, N - 1 do H.check(same(P[i], W[i]), "in-place element " .. i) end

-- Float operands fall back and raise for bitwise.
local F = ffi.new("double[?]", N)
for i = 0, N - 1 do F[i] = i + 0.5 end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
local ok = pcall(function ()
  local d = ffi.new("int32_t[?]", N)
  for i = 0, N - 1 do d[i] = F[i] & B[i] end
  return d
end)
H.check(not ok, "float bitwise operand must raise")
H.eq(jit.stats().tint_hits, before, "float bitwise operand must not use the intrinsic")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/180_j2_cdata_bitwise_pair ok")
