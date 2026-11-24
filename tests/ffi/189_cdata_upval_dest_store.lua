-- ffi/189_cdata_upval_dest_store.lua -- cdata stores whose destination
-- array is a closure upvalue.
--
-- `U[i] = src[i]`, `U[i] = src[i] OP k`, `U[i] = a[i] OP b[i]` and
-- `U[i] = -src[i]/~src[i]` compile through GETUPVAL dest plus the normal
-- body and SETTABLE dest.  The destination is re-read from the closure on
-- every entry.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/189_cdata_upval_dest_store: skipped (no native target)")
  return
end

local N = 200
local A = ffi.new("int32_t[?]", N)
local B = ffi.new("int32_t[?]", N)
for i = 0, N - 1 do A[i] = i * 37 - 400; B[i] = i % 9 end
local U = ffi.new("int32_t[?]", N)

local function copy (n) for i = 0, n - 1 do U[i] = A[i] end end
local function scaled (n) for i = 0, n - 1 do U[i] = A[i] + 3 end end
local function pair (n) for i = 0, n - 1 do U[i] = A[i] + B[i] end end
local function unary (n) for i = 0, n - 1 do U[i] = -A[i] end end
local function bnot (n) for i = 0, n - 1 do U[i] = ~A[i] end end

local function run (name, f, want)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(N)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  for i = 0, N - 1 do H.eq(U[i], want(i), name .. ": element " .. i) end
end

run("copy", copy, function (i) return A[i] end)
run("scaled", scaled, function (i) return A[i] + 3 end)
run("pair", pair, function (i) return A[i] + B[i] end)
run("unary", unary, function (i) return -A[i] end)
run("bnot", bnot, function (i) return ~A[i] end)

-- Rebinding the destination upvalue is observed.
local V = ffi.new("int32_t[?]", N)
U = V
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
pair(N)
H.check(jit.stats().tint_hits > before, "rebound destination did not run")
for i = 0, N - 1 do H.eq(V[i], A[i] + B[i], "rebound element " .. i) end

-- A non-cdata destination falls back and raises normally.
U = 7
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
H.raises(function () copy(N) end, "cdata")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/189_cdata_upval_dest_store ok")
