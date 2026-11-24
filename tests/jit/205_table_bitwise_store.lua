-- jit/205_table_bitwise_store.lua -- native `dst[i] = src[i] OP k`
-- bitwise stores over raw table arrays.
--
-- Bitwise operands must be integers; shifts use Lua's reverse-on-negative
-- and zero-at-64-or-more rules.  Float elements stay on the interpreter,
-- including exact-integral floats, so the helper never guesses a
-- conversion.  Constant, register, immediate and upvalue scalars are
-- covered, as are in-place and strided stores.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/205_table_bitwise_store: skipped (no native target)")
  return
end

local function same (a, b)
  if math.type(a) ~= math.type(b) then return false end
  return a == b
end

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "hotexit=1", "tint=1")
jit.on()

local N = 200
local S = {}
for i = 1, N do S[i] = i * 37 - 400 end
local K = 5
local US = S

local function run (name, f, src, ...)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local want = f({}, src, N, ...)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = f({}, src, N, ...)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  for i = 1, N do
    H.check(same(got[i], want[i]), name .. ": element " .. i)
    H.eq(math.type(got[i]), math.type(want[i]), name .. ": type " .. i)
  end
end

local function band_k (d, s, n) for i = 1, n do d[i] = s[i] & 0x55 end return d end
local function bor_k (d, s, n) for i = 1, n do d[i] = s[i] | 0x100 end return d end
local function bxor_k (d, s, n) for i = 1, n do d[i] = s[i] ~ 0x0f end return d end
local function shl_k (d, s, n) for i = 1, n do d[i] = s[i] << 3 end return d end
local function shr_k (d, s, n) for i = 1, n do d[i] = s[i] >> 3 end return d end
local function band_reg (d, s, n, k) local v = k; for i = 1, n do d[i] = s[i] & v end return d end
local function shl_reg (d, s, n, k) local v = k; for i = 1, n do d[i] = s[i] << v end return d end
local function shr_neg (d, s, n) local v = -3; for i = 1, n do d[i] = s[i] >> v end return d end
local function shl_64 (d, s, n) local v = 64; for i = 1, n do d[i] = s[i] << v end return d end
local function band_up_scalar (d, s, n) for i = 1, n do d[i] = s[i] & K end return d end
local function band_up_src (d, s, n) for i = 1, n do d[i] = US[i] & 7 end return d end
local function shl_step (d, s, n) for i = 1, n, 3 do d[i] = s[i] << 2 end return d end

run("band constant", band_k, S)
run("bor constant", bor_k, S)
run("bxor constant", bxor_k, S)
run("shl constant", shl_k, S)
run("shr constant", shr_k, S)
run("band register", band_reg, S, 0x3c)
run("shl register", shl_reg, S, 4)
run("shr negative", shr_neg, S)
run("shl by 64", shl_64, S)
run("band upvalue scalar", band_up_scalar, S)
run("band upvalue source", band_up_src, S)
run("strided shl", shl_step, S)

-- In-place bitwise store.
local p1, p2 = {}, {}
for i = 1, N do p1[i] = S[i]; p2[i] = S[i] end
local function inplace (a, n) for i = 1, n do a[i] = a[i] ~ 0x55 end return a end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.off()
inplace(p1, N)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
inplace(p2, N)
H.check(jit.stats().tint_hits > before, "in-place bitwise store did not run")
for i = 1, N do H.check(same(p2[i], p1[i]), "in-place element " .. i) end

-- Non-integral float elements stay interpreted and raise.
local F = {}
for i = 1, N do F[i] = i + 0.5 end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
local ok = pcall(function ()
  local d = {}
  for i = 1, N do d[i] = F[i] & 1 end
  return d
end)
H.check(not ok, "float bitwise operand must raise")
H.eq(jit.stats().tint_hits, before, "float bitwise operand must not use the intrinsic")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/205_table_bitwise_store ok")
