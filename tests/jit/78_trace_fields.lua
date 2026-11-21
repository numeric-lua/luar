-- jit/78_trace_fields.lua -- constant-string table fields in J4.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/78_trace_fields: skipped (no native target)")
  return
end

local function empty_write (n)
  local s = 0
  for i = 1, n do
    local t = {}
    t.x = i
    s = s + t.x
  end
  return s
end
local function constructor (n)
  local s = 0
  for i = 1, n do
    local t = { x = i }
    s = s + t.x
  end
  return s
end

jit.flush()
jit.opt.start("ir=1", "alloc=1")
jit.on()
H.eq(empty_write(2000), 2001000)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "field trace did not compile")
H.check(st.ir_executed >= 1, "field trace did not execute")
H.eq(constructor(2000), 2001000)
collectgarbage()
collectgarbage()
H.eq(empty_write(2000), 2001000)

jit.flush()
jit.opt.start("ir=1", "alloc=0")
jit.on()
H.eq(empty_write(2000), 2001000)
H.eq(jit.stats().ir_compiled, 0, "field trace ran without alloc=1")

jit.flush()
jit.opt.start("ir=0", "alloc=0")
jit.on()
print("jit/78_trace_fields ok")
