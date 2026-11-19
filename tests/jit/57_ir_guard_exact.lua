-- jit/57_ir_guard_exact.lua -- failed arithmetic roots resume at the failing op.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/57_ir_guard_exact: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local function f (b, n)
  local s, u = 0, 0
  for i = 1, n do
    s = s + i       -- executed before the float guard fails
    u = u + b
  end
  return s, u
end

local s1, u1 = f(2, 10000)
H.eq(s1, 50005000)
H.eq(u1, 20000)
local s2, u2 = f(2.5, 100)
H.eq(s2, 5050)      -- must not count the first iterations twice
H.eq(u2, 250)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/57_ir_guard_exact ok")
