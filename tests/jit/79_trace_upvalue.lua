-- jit/79_trace_upvalue.lua -- guarded numeric upvalue loads in J4.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/79_trace_upvalue: skipped (no native target)")
  return
end

local factor = 3
local function mul (n)
  local s = 0
  for i = 1, n do s = s + i * factor end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.eq(mul(1000), 1501500)
local st = jit.stats()
H.check(st.ir_compiled >= 1, "numeric upvalue trace did not compile")
H.check(st.ir_executed >= 1, "numeric upvalue trace did not execute")

-- Value changes with the same type: the ULOAD must read the current value.
factor = 4
H.eq(mul(1000), 2002000)

-- Type change: the upvalue tag guard must exit to the interpreter.
factor = 2.5
H.eq(mul(4), 25.0)

-- Closure identity: a second closure of the same Proto must not reuse a
-- trace recorded for the first closure's upvalue.
local function make (k)
  return function (n)
    local s = 0
    for i = 1, n do s = s + i * k end
    return s
  end
end
local f3 = make(3)
jit.flush()
jit.opt.start("ir=1")
jit.on()
H.eq(f3(1000), 1501500)
local f5 = make(5)
H.eq(f5(1000), 2502500)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/79_trace_upvalue ok")
