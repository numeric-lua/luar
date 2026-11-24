-- ffi/193_j2_i64_sum.lua -- J2 reductions over 8-byte integer cdata
-- arrays with a cdata accumulator.
--
-- int64/uint64 add/subtract, register/upvalue arrays and strided/
-- descending runs are covered; the accumulator keeps its cdata type.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/193_j2_i64_sum: skipped (no native target)")
  return
end

local N = 200
local A = ffi.new("int64_t[?]", N)
local U = ffi.new("uint64_t[?]", N)
for i = 0, N - 1 do
  A[i] = i * 3 - 100
  U[i] = i * 2
end

local function run (name, f, acc, want)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = f(acc, N)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(got, want, name .. ": value")
  H.eq(type(got), "cdata", name .. ": cdata result")
end

local function add (acc, n) for i = 0, n - 1 do acc = acc + A[i] end return acc end
local function sub (acc, n) for i = 0, n - 1 do acc = acc - A[i] end return acc end
local function add_u (acc, n) for i = 0, n - 1 do acc = acc + U[i] end return acc end
local function down (acc, n) for i = n - 1, 0, -3 do acc = acc + A[i] end return acc end

local want = ffi.new("int64_t", 0)
for i = 0, N - 1 do want = want + A[i] end
run("int64 add", add, ffi.new("int64_t", 0), want)

want = ffi.new("int64_t", 100000)
for i = 0, N - 1 do want = want - A[i] end
run("int64 sub", sub, ffi.new("int64_t", 100000), want)

local wantu = ffi.new("uint64_t", 0)
for i = 0, N - 1 do wantu = wantu + U[i] end
run("uint64 add", add_u, ffi.new("uint64_t", 0), wantu)

want = ffi.new("int64_t", 0)
for i = N - 1, 0, -3 do want = want + A[i] end
run("descending", down, ffi.new("int64_t", 0), want)

-- Upvalue array.
local UA = A
local function up (acc, n) for i = 0, n - 1 do acc = acc + UA[i] end return acc end
want = ffi.new("int64_t", 0)
for i = 0, N - 1 do want = want + A[i] end
run("upvalue array", up, ffi.new("int64_t", 0), want)

-- Out-of-bounds falls back and raises.
do
  local function over (acc, n) for i = 0, n do acc = acc + A[i] end return acc end
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local ok = pcall(over, ffi.new("int64_t", 0), N)
  H.check(not ok, "out-of-bounds sum must raise")
  H.eq(jit.stats().tint_hits, before, "out-of-bounds sum must not use the helper")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/193_j2_i64_sum ok")
