-- jit/69_call_cont.lua -- the post-CALL suffix can execute natively.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/69_call_cont: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("ir=1")
jit.on()

local y = 1
local function slow (x) return x + y end
local function g (fn, n)
  local s = 0
  for i = 1, n do
    s = s + i
    fn(i)
    s = s + 1
  end
  return s
end

H.eq(g(math.abs, 1000), 501500)
local st = jit.stats()
H.check(st.ir_call_exits >= 1, "C CALL exit not taken")
H.check(st.ir_cont_compiled >= 1, "C continuation not compiled")
H.check(st.ir_cont_runs >= 1, "C continuation not executed")

H.eq(g(slow, 1000), 501500)
st = jit.stats()
H.check(st.ir_cont_runs >= 2, "Lua continuation not executed")

-- the suffix may consume the call result
local function h (fn, n)
  local s = 0
  for i = 1, n do
    s = s + i
    s = s + fn(i)
  end
  return s
end
H.eq(h(math.abs, 1000), 1001000)
st = jit.stats()
H.check(st.ir_cont_runs >= 3, "result-consuming continuation not executed")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/69_call_cont ok")
