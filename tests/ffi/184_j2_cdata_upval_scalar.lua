-- ffi/184_j2_cdata_upval_scalar.lua -- J2 cdata scalar stores with a
-- closure-upvalue scalar.
--
-- The scalar is re-resolved on every entry; changing the upvalue between
-- calls is observed.  Arithmetic and bitwise/scaled forms are covered.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/184_j2_cdata_upval_scalar: skipped (no native target)")
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
local function mul (d, s, n)
  for i = 0, n - 1 do d[i] = s[i] * K end
  return d
end
local function band (d, s, n)
  for i = 0, n - 1 do d[i] = s[i] & K end
  return d
end
local function shl (d, s, n)
  for i = 0, n - 1 do d[i] = s[i] << K end
  return d
end

local function run (name, f, k)
  K = k
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local want = f(ffi.new("int32_t[?]", N), A, N)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = f(ffi.new("int32_t[?]", N), A, N)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  for i = 0, N - 1 do
    H.check(same(got[i], want[i]), name .. ": element " .. i)
    H.eq(math.type(got[i]), math.type(want[i]), name .. ": type " .. i)
  end
end

run("multiply upvalue", mul, 3)
run("multiply rebound", mul, 7)
run("band upvalue", band, 0x5a)
run("band rebound", band, 0x33)
run("shift upvalue", shl, 2)
run("shift rebound", shl, 4)

-- A non-numeric upvalue falls back and raises normally.
K = "x"
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local ok = pcall(function ()
  local d = ffi.new("int32_t[?]", N)
  for i = 0, N - 1 do d[i] = A[i] * K end
  return d
end)
H.check(not ok, "non-numeric upvalue must raise")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/184_j2_cdata_upval_scalar ok")
