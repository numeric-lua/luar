-- ffi/194_j2_i64_bitwise_sum.lua -- J2 bitwise/shift reductions over
-- 8-byte cdata arrays with cdata accumulators.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/194_j2_i64_bitwise_sum: skipped (no native target)")
  return
end

local N = 200
local A = ffi.new("int64_t[?]", N)
local U = ffi.new("uint64_t[?]", N)
for i = 0, N - 1 do
  A[i] = (i % 7) - 3
  U[i] = i % 5
end

local function run (name, f, acc, src, n)
  n = n or N
  jit.off()
  local want = f(acc, src, n)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = f(acc, src, n)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(got, want, name .. ": value")
  H.eq(type(got), "cdata", name .. ": cdata result")
end

local function mkint () return ffi.new("int64_t", 0) end
local function mkuint () return ffi.new("uint64_t", 0) end
local function band (a, s, n) local r = a(); for i = 0, n - 1 do r = r & s[i] end return r end
local function bor (a, s, n) local r = a(); for i = 0, n - 1 do r = r | s[i] end return r end
local function bxor (a, s, n) local r = a(); for i = 0, n - 1 do r = r ~ s[i] end return r end
local function shl (a, s, n) local r = a(); for i = 0, n - 1 do r = r << s[i] end return r end
local function shr (a, s, n) local r = a(); for i = 0, n - 1 do r = r >> s[i] end return r end

run("band", band, function () return ffi.new("int64_t", -1) end, A)
run("bor", bor, function () return ffi.new("int64_t", 0) end, A)
run("bxor", bxor, function () return ffi.new("int64_t", 0) end, A)
run("shl", shl, function () return ffi.new("int64_t", 1) end, A)
run("shr", shr, function () return ffi.new("int64_t", -1) end, A)
run("uint band", band, function () return ffi.new("uint64_t", -1) end, U)

-- Shift counts are masked to six bits, matching cdata shifts.
do
  local C = ffi.new("int64_t[?]", 8)
  C[0], C[1], C[2], C[3] = 0, 1, 63, 64
  C[4], C[5], C[6], C[7] = 65, -1, -64, -65
  local function check (name, f, acc)
    jit.off()
    local want = f(acc, C, 8)
    jit.on()
    local got = f(acc, C, 8)
    H.eq(got, want, name .. ": value")
    H.eq(type(got), "cdata", name .. ": cdata result")
  end
  check("shift mask shl", shl, function () return ffi.new("int64_t", 1) end)
  check("shift mask shr", shr, function () return ffi.new("int64_t", -1) end)
end

-- Upvalue array.
local UA = A
local function up_bor (a, _, n) local r = a(); for i = 0, n - 1 do r = r | UA[i] end return r end
run("upvalue bor", up_bor, function () return ffi.new("int64_t", 0) end, A)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/194_j2_i64_bitwise_sum ok")
