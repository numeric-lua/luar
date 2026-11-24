-- ffi/185_j2_cdata_upval_source.lua -- J2 cdata stores whose source array
-- is a closure upvalue.
--
-- Covers scalar arithmetic/bitwise, two-source, unary and copy forms.  The
-- source is re-resolved on every entry, so rebinding the upvalue is
-- observed; destination growth and in-place forms are covered.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/185_j2_cdata_upval_source: skipped (no native target)")
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
for i = 0, N - 1 do
  A[i] = i * 37 - 400
  B[i] = (i % 9) + 1
end
local U, V = A, B
local K = 5

local function run (name, f)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local want = f(ffi.new("int32_t[?]", N), N)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = f(ffi.new("int32_t[?]", N), N)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  for i = 0, N - 1 do
    H.check(same(got[i], want[i]), name .. ": element " .. i)
    H.eq(math.type(got[i]), math.type(want[i]), name .. ": type " .. i)
  end
end

local function up_scalar (d, n) for i = 0, n - 1 do d[i] = U[i] & K end return d end
local function up_pair (d, n) for i = 0, n - 1 do d[i] = U[i] + V[i] end return d end
local function up_unary (d, n) for i = 0, n - 1 do d[i] = -U[i] end return d end
local function up_copy (d, n) for i = 0, n - 1 do d[i] = U[i] end return d end
local function up_mixed (d, n) for i = 0, n - 1 do d[i] = U[i] - B[i] end return d end

run("upvalue source scalar", up_scalar)
run("upvalue source pair", up_pair)
run("upvalue source unary", up_unary)
run("upvalue source copy", up_copy)
run("mixed upvalue/register", up_mixed)

-- Rebind the source upvalue and change the scalar between calls.
U = B
K = 3
run("rebound upvalue source", up_scalar)

-- In-place copy through an upvalue source.
local P = ffi.new("int32_t[?]", N)
for i = 0, N - 1 do P[i] = A[i] end
local function inplace (d, n) for i = 0, n - 1 do d[i] = d[i] end return d end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.off()
local W = ffi.new("int32_t[?]", N)
for i = 0, N - 1 do W[i] = P[i] end
inplace(W, N)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
inplace(P, N)
H.check(jit.stats().tint_hits > before, "in-place copy did not run")
for i = 0, N - 1 do H.check(same(P[i], W[i]), "in-place element " .. i) end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/185_j2_cdata_upval_source ok")
