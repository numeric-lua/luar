-- jit/92_trace_table_len.lua -- table length in traces.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/92_trace_table_len: skipped (no native target)")
  return
end

local function f_on (n)
  local t = { 1, 2, 3 }
  local x = 0
  for _ = 1, n do x = x + #t end
  return x
end
local function f_off (n)
  local t = { 1, 2, 3 }
  local x = 0
  for _ = 1, n do x = x + #t end
  return x
end

jit.flush()
jit.opt.start("ir=1", "alloc=1")
jit.on()
H.eq(f_on(2000), 6000)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "table length trace did not compile")
H.check(st.ir_executed >= 1, "table length trace did not execute")
collectgarbage()
collectgarbage()
H.eq(f_on(2000), 6000)

-- __len must stay interpreter-side.
local mt = { __len = function () return 7 end }
local function f_meta (n)
  local t = setmetatable({ 1, 2 }, mt)
  local x = 0
  for _ = 1, n do x = x + #t end
  return x
end
H.eq(f_meta(100), 700)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.eq(f_off(2000), 6000)
H.check(jit.stats().ir_executed >= 1, "table length did not trace with alloc=0")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/92_trace_table_len ok")
