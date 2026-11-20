-- jit/70_call_yield.lua -- yields inside traced calls keep continuations safe.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/70_call_yield: skipped (no native target)")
  return
end

jit.flush()
jit.opt.start("ir=1")
jit.on()

local function y (x)
  coroutine.yield(x)
  return x
end
local function g (fn, n)
  local s = 0
  for i = 1, n do
    s = s + i
    fn(i)
    s = s + 1
  end
  return s
end

local co = coroutine.create(function () return g(y, 100) end)
local resumes = 0
local ret
while true do
  local ok, v = coroutine.resume(co)
  H.check(ok, tostring(v))
  resumes = resumes + 1
  if coroutine.status(co) == "dead" then
    ret = v
    break
  end
end
H.eq(ret, 5150)
H.eq(resumes, 101)
H.check(jit.stats().ir_cont_runs >= 1, "continuation did not run after yield")

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("jit/70_call_yield ok")
