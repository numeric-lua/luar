-- jit/45_ir_prefix_reject.lua -- prefixed table bodies must stay in the interpreter.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/45_ir_prefix_reject: skipped (no native target)")
  return
end

local t = {}
jit.off()
for i = 1, 20001 do t[i] = i end
jit.on()

jit.flush()
jit.opt.start("ir=1")
jit.on()
local function read (t, n)
  local s = 0
  for i = 1, n do
    local x = i + 1
    s = s + t[x] * 2
  end
  return s
end

H.eq(read(t, 20000), 400060000)
H.eq(jit.stats().ir_executed, 0)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/45_ir_prefix_reject ok")
