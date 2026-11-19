-- jit/55_ir_float_table.lua -- live float table roots.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/55_ir_float_table: skipped (no native target)")
  return
end

local t = {}
jit.off()
for i = 1, 1000 do t[i] = i + 0.5 end
jit.on()

jit.flush()
jit.opt.start("ir=1")
jit.on()
local function sum (t, n)
  local s = 0
  for i = 1, n do s = s + t[i] * 2 end
  return s
end

H.eq(sum(t, 1000), 1002000.0)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "float table IR root was not compiled")
H.check(st.ir_executed >= 1, "float table IR root was not executed")

-- float constants must retain their double value (not integer imul 0)
local function sumf (t, n)
  local s = 0
  for i = 1, n do s = s + t[i] * 2.0 end
  return s
end
H.eq(sumf(t, 1000), 1002000.0)


jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/55_ir_float_table ok")
