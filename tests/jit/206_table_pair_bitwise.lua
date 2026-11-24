-- jit/206_table_pair_bitwise.lua -- native `dst[i] = a[i] OP b[i]`
-- bitwise stores over two raw table arrays.
--
-- Both sources must hold integers; shifts use Lua's reverse-on-negative
-- and zero-at-64 rules.  Float sources stay interpreted.  In-place,
-- upvalue and strided forms are covered.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/206_table_pair_bitwise: skipped (no native target)")
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
local A, B = {}, {}
for i = 1, N do
  A[i] = i * 37 - 400
  B[i] = (i % 9) + 1
end
local UA, UB = A, B

local function run (name, f, a, b)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local want = f({}, a, b, N)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = f({}, a, b, N)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  for i = 1, N do
    H.check(same(got[i], want[i]), name .. ": element " .. i)
    H.eq(math.type(got[i]), math.type(want[i]), name .. ": type " .. i)
  end
end

local function band (d, a, b, n) for i = 1, n do d[i] = a[i] & b[i] end return d end
local function bor (d, a, b, n) for i = 1, n do d[i] = a[i] | b[i] end return d end
local function bxor (d, a, b, n) for i = 1, n do d[i] = a[i] ~ b[i] end return d end
local function shl (d, a, b, n) for i = 1, n do d[i] = a[i] << b[i] end return d end
local function shr (d, a, b, n) for i = 1, n do d[i] = a[i] >> b[i] end return d end
local function shl_step (d, a, b, n) for i = 1, n, 3 do d[i] = a[i] << b[i] end return d end
local function up_band (d, a, b, n) for i = 1, n do d[i] = UA[i] & UB[i] end return d end

run("band", band, A, B)
run("bor", bor, A, B)
run("bxor", bxor, A, B)
run("shl", shl, A, B)
run("shr", shr, A, B)
run("strided shl", shl_step, A, B)
run("upvalue sources", up_band, A, B)

-- Negative shift counts reverse direction; 64 clears the result.
local NEG, BIG = {}, {}
for i = 1, N do NEG[i] = -3; BIG[i] = 64 end
run("negative shift", shl, A, NEG)
run("shift by 64", shl, A, BIG)

-- In-place destination is also the first source.
local p1, p2 = {}, {}
for i = 1, N do p1[i] = A[i]; p2[i] = A[i] end
local function inplace (a, b, n) for i = 1, n do a[i] = a[i] & b[i] end return a end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.off()
inplace(p1, B, N)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
inplace(p2, B, N)
H.check(jit.stats().tint_hits > before, "in-place bitwise store did not run")
for i = 1, N do H.check(same(p2[i], p1[i]), "in-place element " .. i) end

-- Float sources stay interpreted.
local F = {}
for i = 1, N do F[i] = i + 0.5 end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
local ok = pcall(function ()
  local d = {}
  for i = 1, N do d[i] = F[i] & B[i] end
  return d
end)
H.check(not ok, "float bitwise operand must raise")
H.eq(jit.stats().tint_hits, before, "float bitwise operand must not use the intrinsic")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/206_table_pair_bitwise ok")
