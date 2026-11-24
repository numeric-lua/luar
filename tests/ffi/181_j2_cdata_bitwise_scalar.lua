-- ffi/181_j2_cdata_bitwise_scalar.lua -- J2 `a[i] = b[i] OP k` bitwise
-- and shift stores over bounded cdata arrays.
--
-- Constant, register, immediate and upvalue scalars are supported.
-- Negative shift counts reverse, 64 or more yields zero, and float
-- operands stay on the interpreter.  In-place, descending and strided
-- ranges are covered.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/181_j2_cdata_bitwise_scalar: skipped (no native target)")
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
for i = 0, N - 1 do A[i] = i * 37 - 400 end
local K = 5
local UA = A

local function run (name, f, src, ...)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local want = f(ffi.new("int32_t[?]", N), src, N, ...)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = f(ffi.new("int32_t[?]", N), src, N, ...)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  for i = 0, N - 1 do
    H.check(same(got[i], want[i]), name .. ": element " .. i)
    H.eq(math.type(got[i]), math.type(want[i]), name .. ": type " .. i)
  end
end

local function band_k (d, s, n) for i = 0, n - 1 do d[i] = s[i] & 0x55 end return d end
local function bor_k (d, s, n) for i = 0, n - 1 do d[i] = s[i] | 0x100 end return d end
local function bxor_k (d, s, n) for i = 0, n - 1 do d[i] = s[i] ~ 0x0f end return d end
local function shl_i (d, s, n) for i = 0, n - 1 do d[i] = s[i] << 3 end return d end
local function shr_i (d, s, n) for i = 0, n - 1 do d[i] = s[i] >> 3 end return d end
local function band_reg (d, s, n, k) local v = k; for i = 0, n - 1 do d[i] = s[i] & v end return d end
local function shl_reg (d, s, n, k) local v = k; for i = 0, n - 1 do d[i] = s[i] << v end return d end
local function shr_neg (d, s, n) local v = -3; for i = 0, n - 1 do d[i] = s[i] >> v end return d end
local function shl_64 (d, s, n) local v = 64; for i = 0, n - 1 do d[i] = s[i] << v end return d end
local function shl_step (d, s, n) for i = 0, n - 1, 3 do d[i] = s[i] << 2 end return d end

run("band constant", band_k, A)
run("bor constant", bor_k, A)
run("bxor constant", bxor_k, A)
run("shl immediate", shl_i, A)
run("shr immediate", shr_i, A)
run("band register", band_reg, A, 0x3c)
run("shl register", shl_reg, A, 4)
run("shr negative", shr_neg, A)
run("shl by 64", shl_64, A)
run("strided shl", shl_step, A)

-- In-place bitwise store.
local P = ffi.new("int32_t[?]", N)
for i = 0, N - 1 do P[i] = A[i] end
local W = ffi.new("int32_t[?]", N)
for i = 0, N - 1 do W[i] = A[i] end
local function inplace (d, s, n) for i = 0, n - 1 do d[i] = d[i] ~ 0x55 end return d end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.off()
inplace(W, nil, N)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
inplace(P, nil, N)
H.check(jit.stats().tint_hits > before, "in-place bitwise store did not run")
for i = 0, N - 1 do H.check(same(P[i], W[i]), "in-place element " .. i) end

-- Float operands fall back and raise.
local F = ffi.new("double[?]", N)
for i = 0, N - 1 do F[i] = i + 0.5 end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
local ok = pcall(function ()
  local d = ffi.new("int32_t[?]", N)
  for i = 0, N - 1 do d[i] = F[i] & 1 end
  return d
end)
H.check(not ok, "float bitwise operand must raise")
H.eq(jit.stats().tint_hits, before, "float bitwise operand must not use the intrinsic")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/181_j2_cdata_bitwise_scalar ok")
