-- jit/56_ir_call_exit.lua -- non-inlineable CALL exits leave the interpreter the frame.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/56_ir_call_exit: skipped (no native target)")
  return
end

local t = {}
jit.off()
for i = 1, 1000 do t[i] = i end
jit.on()

jit.flush()
jit.opt.start("ir=1")
jit.on()

local y = 10
local function f (x) return x + y end   -- upvalue: not inlineable
local function g (fn, t, n)
  local s = 0
  for i = 1, n do s = s + t[i] + fn(i) end
  return s
end

H.eq(g(f, t, 1000), 500500 + 500500 + 10000)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "CALL-prefix IR was not compiled")
H.check(st.ir_call_exits >= 1, "CALL exit was not taken")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/56_ir_call_exit ok")
