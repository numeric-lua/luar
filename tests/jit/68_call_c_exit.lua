-- jit/68_call_c_exit.lua -- CALL exits cover C functions and callable tables.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/68_call_c_exit: skipped (no native target)")
  return
end

local function g (fn, n)
  local s = 0
  for i = 1, n do
    s = s + i
    fn(i)
  end
  return s
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
H.eq(g(math.abs, 1000), 500500)
H.check(jit.stats().ir_call_exits >= 1, "C function CALL exit not taken")

local callable = setmetatable({}, {
  __call = function (self, x) return -x end,
})
H.eq(g(callable, 1000), 500500)
H.check(jit.stats().ir_call_exits >= 2, "callable CALL exit not taken")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/68_call_c_exit ok")
