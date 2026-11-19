-- jit/59_side_chain.lua -- a child side covers a second closure identity.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/59_side_chain: skipped (no native target)")
  return
end

local function f0 (x) return x * 2 end
local function f1 (x) return x * 3 end
local function g0 (x) return x + 1 end
local function g1 (x) return x + 2 end
local function sum (f, g, n)
  local s = 0
  for i = 1, n do s = s + f(i) + g(i) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()

local n = 2000
H.eq(sum(f0, g0, n), 3 * (n * (n + 1) / 2) + n)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "root not compiled")

-- first closure changes: one side
H.eq(sum(f1, g0, n), 4 * (n * (n + 1) / 2) + n)
st = jit.stats()
H.check(st.ir_side_traces >= 1, "first side not recorded")

-- second closure changes: the side must chain to a child
H.eq(sum(f1, g1, n), 4 * (n * (n + 1) / 2) + 2 * n)
st = jit.stats()
H.check(st.ir_side_traces >= 2, "child side not recorded")
H.check(st.ir_side_depth >= 2, "child depth not tracked")
H.check(st.ir_side_executed >= 1, "chained side not executed")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/59_side_chain ok")
