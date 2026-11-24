-- ffi/191_j2_i64_copy.lua -- J2 raw 64-bit cdata copies.
--
-- int64_t/uint64_t element copies use an 8-byte helper and expose a cdata
-- element temporary.  Register/upvalue sources and destinations, plus
-- strided/descending runs, are covered.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/191_j2_i64_copy: skipped (no native target)")
  return
end

local N = 200
local A = ffi.new("int64_t[?]", N)
local B = ffi.new("uint64_t[?]", N)
for i = 0, N - 1 do
  A[i] = i * 1000000007 - 999
  B[i] = i * 3
end

local function run (name, f, src, want)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local dst = ffi.new("int64_t[?]", N)
  f(dst, src, N)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  for i = 0, N - 1 do H.eq(dst[i], want(i), name .. ": element " .. i) end
  H.eq(type(dst[0]), "cdata", name .. ": cdata element")
end

local function copy (d, s, n) for i = 0, n - 1 do d[i] = s[i] end end
local function copy_stride (d, s, n) for i = 0, n - 1, 3 do d[i] = s[i] end end
local function copy_down (d, s, n) for i = n - 1, 0, -2 do d[i] = s[i] end end

run("int64", copy, A, function (i) return A[i] end)
run("uint64", copy, B, function (i) return B[i] end)
run("stride", copy_stride, A, function (i)
  if i % 3 ~= 0 then return ffi.new("int64_t", 0) end
  return A[i]
end)
run("descending", copy_down, A, function (i)
  if (N - 1 - i) % 2 ~= 0 then return ffi.new("int64_t", 0) end
  return A[i]
end)

-- Upvalue source.
local UA = A
local function copy_up_src (d, _, n)
  for i = 0, n - 1 do d[i] = UA[i] end
end
run("upvalue source", copy_up_src, A, function (i) return A[i] end)

-- Upvalue destination.
local U = ffi.new("int64_t[?]", N)
local function copy_up_dst (src, n)
  for i = 0, n - 1 do U[i] = src[i] end
end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
copy_up_dst(A, N)
H.check(jit.stats().tint_hits > before, "upvalue destination did not run")
for i = 0, N - 1 do H.eq(U[i], A[i], "upvalue destination " .. i) end

-- int64 -> uint64 destination preserves the raw two's-complement bits.
do
  local U = ffi.new("uint64_t[?]", N)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local function to_u (d, s, n) for i = 0, n - 1 do d[i] = s[i] end end
  to_u(U, A, N)
  H.check(jit.stats().tint_hits > before, "uint64 destination did not run")
  for i = 0, N - 1 do H.eq(U[i], ffi.cast("uint64_t", A[i]), "uint64 dest " .. i) end
end

-- Self-copy is a no-op and stays on the raw helper.
do
  local S = ffi.new("int64_t[?]", N)
  for i = 0, N - 1 do S[i] = A[i] end
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local function self (d, n) for i = 0, n - 1 do d[i] = d[i] end end
  self(S, N)
  H.check(jit.stats().tint_hits > before, "self-copy did not run")
  for i = 0, N - 1 do H.eq(S[i], A[i], "self-copy " .. i) end
end

-- Out-of-bounds runs fall back and raise from the interpreter.
do
  local function over (d, s, n) for i = 0, n do d[i] = s[i] end end
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local ok = pcall(function ()
    local d = ffi.new("int64_t[?]", N)
    over(d, A, N)
  end)
  H.check(not ok, "out-of-bounds i64 copy must raise")
  H.eq(jit.stats().tint_hits, before, "out-of-bounds copy must not use the helper")
end

-- The visible element temporary is a cdata int64, not pointer bits.
local function copy_temp (d, s, n)
  local x
  for i = 0, n - 1 do
    x = s[i]
    d[i] = x
  end
  return x
end
local D2 = ffi.new("int64_t[?]", N)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
local x = copy_temp(D2, A, N)
H.check(jit.stats().tint_hits > before, "visible temp copy did not run")
H.eq(type(x), "cdata", "visible temp cdata")
H.eq(x, A[N - 1], "visible temp value")
for i = 0, N - 1 do H.eq(D2[i], A[i], "visible temp destination " .. i) end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/191_j2_i64_copy ok")
