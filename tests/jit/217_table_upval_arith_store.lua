-- jit/217_table_upval_arith_store.lua -- arithmetic table stores whose
-- destination is a closure-upvalue table.
--
-- `U[i] = src[i] OP k`, `U[i] = a[i] OP b[i]` and `U[i] = -src[i]/~src[i]`
-- compile through GETUPVAL dest plus the normal body and SETTABLE dest.
-- The destination is re-read from the closure on every entry.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/217_table_upval_arith_store: skipped (no native target)")
  return
end

local N = 200
local A, B = {}, {}
for i = 1, N do A[i] = i; B[i] = i * 2 end
local U = {}

local function scaled (n)
  for i = 1, n do U[i] = A[i] * 3 end
end
local function pair (n)
  for i = 1, n do U[i] = A[i] + B[i] end
end
local function unary (n)
  for i = 1, n do U[i] = -A[i] end
end
local function bnot (n)
  for i = 1, n do U[i] = ~A[i] end
end

local function run (name, f)
  U = {}
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(N)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  for i = 1, N do
    local want
    if name == "scaled" then want = A[i] * 3
    elseif name == "pair" then want = A[i] + B[i]
    elseif name == "unary" then want = -A[i]
    else want = ~A[i] end
    H.eq(U[i], want, name .. ": element " .. i)
  end
end

run("scaled", scaled)
run("pair", pair)
run("unary", unary)
run("bnot", bnot)

-- Rebinding the destination upvalue is observed.
local V = {}
U = V
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
pair(N)
H.check(jit.stats().tint_hits > before, "rebound destination did not run")
for i = 1, N do H.eq(V[i], A[i] + B[i], "rebound element " .. i) end

-- A non-table destination falls back and raises normally.
U = 7
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
H.raises(function () pair(N) end, "table")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/217_table_upval_arith_store ok")
