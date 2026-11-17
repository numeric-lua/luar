-- jit/18_partial_call.lua -- numeric prefixes that hand a CALL back to the VM.
local H = dofile("tests/helpers.lua")

jit.flush()
jit.on()
jit.opt.start("call=1")
if jit.status() ~= true then
  print("jit/18_partial_call: skipped (no native target)")
  return
end

local calls = 0
local function cb (x)
  calls = calls + 1
  return x
end

local function loop_off (f, n)
  local s = 0
  for i = 1, n do
    s = s + i
    f(i)
  end
  return s
end
local function loop_on (f, n)
  local s = 0
  for i = 1, n do
    s = s + i
    f(i)
  end
  return s
end

jit.off()
local want = loop_off(cb, 1000)
local off_calls = calls
calls = 0
jit.on()
local got = loop_on(cb, 1000)
H.eq(got, want)
H.eq(calls, off_calls)
H.eq(calls, 1000)
H.check(jit.stats().compiled >= 1, "partial call prefix did not compile")
H.check(jit.stats().traces >= 1, "partial call trace missing")

-- repeat/until has no condition branch before the call, so its prefix
-- also hands the CALL back to the interpreter
local calls2 = 0
local function cb2 (x) calls2 = calls2 + 1; return x end
local function rep_off (f, n)
  local i, s = 0, 0
  repeat s = s + i; f(i); i = i + 1 until i >= n
  return s
end
local function rep_on (f, n)
  local i, s = 0, 0
  repeat s = s + i; f(i); i = i + 1 until i >= n
  return s
end
jit.off()
local rwant = rep_off(cb2, 1000)
local rcalls = calls2
calls2 = 0
jit.on()
H.eq(rep_on(cb2, 1000), rwant)
H.eq(calls2, rcalls)

-- a branch before the call leaves the prefix unimplemented; fall back
local function branchy (f, n)
  local s = 0
  for i = 1, n do
    if i % 2 == 0 then s = s + i end
    f(i)
  end
  return s
end
jit.off()
local bwant = branchy(cb, 100)
jit.on()
H.eq(branchy(cb, 100), bwant)

jit.off()
H.eq(loop_on(cb, 100), loop_off(cb, 100))
jit.on()
jit.opt.start("call=0")

print("jit/18_partial_call ok")
