-- jit/49_ir_calls.lua -- inline one-argument integer leaf calls into IR roots.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/49_ir_calls: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("ir=1")
jit.on()

local function dbl (x) return x * 2 end
local function bump (x) return x + 3 end
local function sum (f, n)
  local s = 0
  for i = 1, n do s = s + f(i) end
  return s
end

H.eq(sum(dbl, 10000), 100010000)
H.eq(sum(bump, 10000), 50035000)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "call-loop IR root was not compiled")
H.check(st.ir_executed >= 1, "call-loop IR root was not executed")

-- two-argument leaf calls are inlined as well
local function add2 (x, y) return x + y end
local function sum2 (f, n)
  local s = 0
  for i = 1, n do s = s + f(i, 1) end
  return s
end
local before = jit.stats().ir_compiled
H.eq(sum2(add2, 10000), 50015000)
H.check(jit.stats().ir_compiled > before,
        "two-argument leaf call was not compiled")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/49_ir_calls ok")
