-- jit/40_ir_fallback.lua -- unsupported IR traces must fall back safely.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/40_ir_fallback: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("ir=1")
jit.on()

-- table writes are not emitted by the current IR compiler
local t = {}
local function fill (t, n)
  for i = 1, n do t[i] = i end
end
fill(t, 1000)
H.eq(t[1000], 1000)
H.eq(jit.stats().ir_executed, 0)

-- a fresh integer loop in the same mode still compiles and executes
local function ints (n)
  local s = 0
  for i = 1, n do s = s + i end
  return s
end
H.eq(ints(1000), 500500)
H.check(jit.stats().ir_executed >= 1, "integer IR trace did not execute")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/40_ir_fallback ok")
