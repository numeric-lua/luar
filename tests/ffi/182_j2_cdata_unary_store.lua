-- ffi/182_j2_cdata_unary_store.lua -- J2 `a[i] = -b[i]` and
-- `a[i] = ~b[i]` over bounded cdata arrays.
--
-- Unary minus keeps integer wrap or flips the float sign; bitwise not
-- requires integer elements.  In-place, descending and strided ranges are
-- covered, and the element temp keeps its last value.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/182_j2_cdata_unary_store: skipped (no native target)")
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
local F = ffi.new("double[?]", N)
for i = 0, N - 1 do
  A[i] = i * 37 - 400
  F[i] = i - 100 + 0.5
end

local function run (name, f, src, vtype)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local want = f(ffi.new(vtype, N), src, N)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = f(ffi.new(vtype, N), src, N)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  for i = 0, N - 1 do
    H.check(same(got[i], want[i]), name .. ": element " .. i)
    H.eq(math.type(got[i]), math.type(want[i]), name .. ": type " .. i)
  end
end

local function neg (d, s, n) for i = 0, n - 1 do d[i] = -s[i] end return d end
local function bnot (d, s, n) for i = 0, n - 1 do d[i] = ~s[i] end return d end
local function neg_down (d, s, n) for i = n - 1, 0, -1 do d[i] = -s[i] end return d end
local function bnot_step (d, s, n) for i = 0, n - 1, 3 do d[i] = ~s[i] end return d end

run("unary minus integer", neg, A, "int32_t[?]")
run("unary minus float", neg, F, "double[?]")
run("bitwise not", bnot, A, "int32_t[?]")
run("descending unary minus", neg_down, A, "int32_t[?]")
run("strided bitwise not", bnot_step, A, "int32_t[?]")

-- In-place unary minus.
local W = ffi.new("int32_t[?]", N)
local P = ffi.new("int32_t[?]", N)
for i = 0, N - 1 do W[i] = A[i]; P[i] = A[i] end
local function inplace (d, s, n) for i = 0, n - 1 do d[i] = -d[i] end return d end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.off()
inplace(W, nil, N)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
inplace(P, nil, N)
H.check(jit.stats().tint_hits > before, "in-place unary minus did not run")
for i = 0, N - 1 do H.check(same(P[i], W[i]), "in-place element " .. i) end

-- Float bitwise not falls back and raises.
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
local ok = pcall(function ()
  local d = ffi.new("int32_t[?]", N)
  for i = 0, N - 1 do d[i] = ~F[i] end
  return d
end)
H.check(not ok, "float bitwise not must raise")
H.eq(jit.stats().tint_hits, before, "float bitwise not must not use the intrinsic")

-- Element temp is observable after the loop.
local function observe (d, s, n)
  local x
  for i = 0, n - 1 do
    x = -s[i]
    d[i] = x
  end
  return d, x
end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.off()
local wd, wx = observe(ffi.new("int32_t[?]", N), A, N)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
local gd, gx = observe(ffi.new("int32_t[?]", N), A, N)
H.check(jit.stats().tint_hits > before, "observed unary store did not run")
H.check(same(gx, wx), "observed element temp")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/182_j2_cdata_unary_store ok")
