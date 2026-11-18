-- jit/42_ir_table_write.lua -- live table-write IR traces.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/42_ir_table_write: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local t = table.new and table.new(1000) or {}
local function fill (t, n)
  for i = 1, n do t[i] = i end
end
fill(t, 1000)
H.eq(t[1], 1)
H.eq(t[1000], 1000)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "table-write IR trace was not compiled")
H.check(st.ir_executed >= 1, "table-write IR trace was not executed")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/42_ir_table_write ok")
