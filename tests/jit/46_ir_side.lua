-- jit/46_ir_side.lua -- side traces linked from IR root guard exits.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/46_ir_side: skipped (no native target)")
  return
end

local t = {}
jit.off()
for i = 1, 20000 do t[i] = i end
jit.on()

jit.flush()
jit.opt.start("ir=1")
jit.on()
local function read (t, n)
  local s = 0
  for i = 1, n do s = s + t[i] * 2 end
  return s
end

H.eq(read(t, 20000), 400020000)

local u = table.new(0, 20000)
for i = 1, 20000 do u[i] = i end
H.eq(read(u, 20000), 400020000)

local st = jit.stats()
H.check(st.ir_side_traces >= 1, "IR side trace was not recorded")
H.check(st.ir_side_compiled >= 1, "IR side trace was not compiled")
H.check(st.ir_side_executed >= 1, "IR side trace was not executed")
H.check(st.ir_cycles >= 1, "root-start side exit was not classified as a cycle")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/46_ir_side ok")
