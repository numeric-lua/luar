-- jit/106_trace_boolean_field_read.lua -- boolean table-field reads.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/106_trace_boolean_field_read: skipped (no native target)")
  return
end

local function f_on (n)
  local t = { x = true, y = false }
  local last, s = nil, 0
  for _ = 1, n do
    last = t.x
    s = s + 1
  end
  return s, last
end
local function f_off (n)
  local t = { x = true }
  local last, s = nil, 0
  for _ = 1, n do
    last = t.x
    s = s + 1
  end
  return s, last
end

jit.flush()
jit.opt.start("ir=1", "alloc=1")
jit.on()
local s, b = f_on(2000)
H.eq(s, 2000)
H.eq(b, true)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "boolean field read did not compile")
H.check(st.ir_executed >= 1, "boolean field read did not execute")
collectgarbage()
collectgarbage()
H.eq(b, true)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
s, b = f_off(2000)
H.eq(s, 2000)
H.eq(b, true)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/106_trace_boolean_field_read ok")
