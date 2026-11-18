-- jit/41_ir_table.lua -- live table-read IR traces.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/41_ir_table: skipped (no native target)")
  return
end

local t = {}
jit.off()
for i = 1, 1000 do t[i] = i end
jit.on()

jit.flush()
jit.opt.start("ir=1")
jit.on()
local function read (t, n)
  local s = 0
  for i = 1, n do s = s + t[i] end
  return s
end

H.eq(read(t, 1000), 500500)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "table IR trace was not compiled")
H.check(st.ir_executed >= 1, "table IR trace was not executed")

-- a non-integer element must fall back and raise the interpreter error
t[500] = "bad"
H.eq(pcall(function () return read(t, 1000) end), false)
t[500] = 500

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/41_ir_table ok")
