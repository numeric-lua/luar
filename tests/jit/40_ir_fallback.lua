-- jit/40_ir_fallback.lua -- unsupported IR traces must fall back safely.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/40_ir_fallback: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("ir=1")
jit.on()

-- table reads are not emitted by the current integer IR compiler
local t = {}
jit.off()
for i = 1, 1000 do t[i] = i end
jit.on()
local function read (t, n)
  local s = 0
  for i = 1, n do s = s + t[i] end
  return s
end
H.eq(read(t, 1000), 500500)
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
