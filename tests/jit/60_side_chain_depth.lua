-- jit/60_side_chain_depth.lua -- chains reach depth 4 and cap there.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/60_side_chain_depth: skipped (no native target)")
  return
end

local function a0 (x) return x * 2 end
local function a1 (x) return x * 3 end
local function b0 (x) return x + 1 end
local function b1 (x) return x + 2 end
local function c0 (x) return x - 1 end
local function c1 (x) return x - 2 end
local function d0 (x) return x + 3 end
local function d1 (x) return x + 4 end

local function sum (a, b, c, d, n)
  local s = 0
  for i = 1, n do s = s + a(i) + b(i) + c(i) + d(i) end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local n = 1000
local S = n * (n + 1) / 2
H.eq(sum(a0, b0, c0, d0, n), 5 * S + 3 * n)
H.eq(sum(a1, b0, c0, d0, n), 6 * S + 3 * n)
H.eq(sum(a1, b1, c0, d0, n), 6 * S + 4 * n)
H.eq(sum(a1, b1, c1, d0, n), 6 * S + 3 * n)
H.eq(sum(a1, b1, c1, d1, n), 6 * S + 4 * n)
local st = jit.stats()
H.check(st.ir_side_traces >= 4, "depth-4 chain was not recorded")
H.check(st.ir_max_depth >= 4, "side depth 4 was not reached")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/60_side_chain_depth ok")
