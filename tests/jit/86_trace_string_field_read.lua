-- jit/86_trace_string_field_read.lua -- string table-field reads in J4.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/86_trace_string_field_read: skipped (no native target)")
  return
end

local function f_on (n)
  local t = { x = "hello" }
  local last, s = "", 0
  for _ = 1, n do
    last = t.x
    s = s + 1
  end
  return s, last
end
local function f_off (n)
  local t = { x = "hello" }
  local last, s = "", 0
  for _ = 1, n do
    last = t.x
    s = s + 1
  end
  return s, last
end

jit.flush()
jit.opt.start("ir=1", "alloc=1")
jit.on()
local s, last = f_on(2000)
H.eq(s, 2000)
H.eq(last, "hello")
local st = jit.stats()
H.check(st.ir_compiled >= 1, "string field read trace did not compile")
H.check(st.ir_executed >= 1, "string field read trace did not execute")
collectgarbage()
collectgarbage()
H.eq(last, "hello")

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
s, last = f_off(2000)
H.eq(s, 2000)
H.eq(last, "hello")

jit.flush()
jit.opt.start("ir=0", "alloc=0")
jit.on()
print("jit/86_trace_string_field_read ok")
