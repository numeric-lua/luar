-- ffi/195_j2_i64_divmod_sum.lua -- J2 64-bit division/modulo reductions
-- into cdata accumulators.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/195_j2_i64_divmod_sum: skipped (no native target)")
  return
end

local N = 200
local A = ffi.new("int64_t[?]", N)
local U = ffi.new("uint64_t[?]", N)
for i = 0, N - 1 do
  A[i] = (i % 9) - 4
  if A[i] == 0 then A[i] = 1 end
  U[i] = (i % 7) + 1
end

local function run (name, f, acc, src)
  jit.off()
  local want = f(acc, src, N)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = f(acc, src, N)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(got, want, name .. ": value")
  H.eq(type(got), "cdata", name .. ": cdata result")
end

local function idiv (a, s, n) local r = a; for i = 0, n - 1 do r = r // s[i] end return r end
local function imod (a, s, n) local r = a; for i = 0, n - 1 do r = r % s[i] end return r end

run("int64 //", idiv, ffi.new("int64_t", 1000000), A)
run("int64 %", imod, ffi.new("int64_t", 1000000), A)
run("uint64 //", idiv, ffi.new("uint64_t", 1000000), U)
run("uint64 %", imod, ffi.new("uint64_t", 1000000), U)

-- Strided and descending forms.
do
  local function down (a, s, n) local r = a; for i = n - 1, 0, -3 do r = r // s[i] end return r end
  jit.off()
  local want = down(ffi.new("int64_t", 1000000), A, N)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  H.eq(down(ffi.new("int64_t", 1000000), A, N), want, "descending //")
  H.check(jit.stats().tint_hits > before, "descending // did not run")
end

-- A zero divisor falls back to the interpreter, which returns INT64_MIN
-- for a cdata integer division by zero.
do
  local Z = ffi.new("int64_t[?]", N)
  for i = 0, N - 1 do Z[i] = 1 end
  Z[10] = 0
  jit.off()
  local want = idiv(ffi.new("int64_t", 1000000), Z, N)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  H.eq(idiv(ffi.new("int64_t", 1000000), Z, N), want, "zero divisor")
  H.check(jit.stats().tint_hits >= before, "zero divisor counters")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/195_j2_i64_divmod_sum ok")
