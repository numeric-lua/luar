-- jit/165_pair_array_sum.lua -- native two-array sum and difference.
--
-- `s = s + a[i] + b[i]` and `s = s + a[i] - b[i]` are common numeric
-- kernels.  The generic table emitter only covers register tables and is
-- opt-in; the pair intrinsic runs both register and upvalue arrays in one C
-- pass, preserving the element/temporary registers the bytecode can observe
-- after the loop.  The expected values come from the interpreter, and all
-- shapes are deterministic.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/165_pair_array_sum: skipped (no native target)")
  return
end

local N = 300
local upA, upB = {}, {}
for i = 1, N do
  upA[i] = i
  upB[i] = i * 2
end

local regA, regB = {}, {}
for i = 1, N do
  regA[i] = i * 3
  regB[i] = i - 100
end

local mixedA, mixedB = {}, {}
for i = 1, N do
  mixedA[i] = (i % 19 == 0) and (i + 0.25) or i
  mixedB[i] = (i % 23 == 0) and (i - 0.75) or i * 2
end

local bigA, bigB = {}, {}
for i = 1, N do
  bigA[i] = math.maxinteger - i
  bigB[i] = i * 1000003
end

local function run (f, a, b)
  jit.flush()
  jit.off()
  local ok1, s1, x1 = pcall(f, a, b, N)
  jit.flush()
  jit.on()
  local ok2, s2, x2 = pcall(f, a, b, N)
  local st = jit.stats()
  H.eq(ok1, ok2, "error status")
  if ok1 then
    H.eq(s1, s2, "sum")
    H.eq(x1, x2, "result register")
  else
    H.eq(tostring(s1), tostring(s2), "error message")
  end
  return st
end

local function sum_up (a, b, n)
  local s = 0
  for i = 1, n do s = s + upA[i] + upB[i] end
  return s
end

local function sub_up (a, b, n)
  local s = 0
  for i = 1, n do s = s + upA[i] - upB[i] end
  return s
end

local function sum_argv (a, b, n)
  local s = 0
  for i = 1, n do s = s + a[i] + b[i] end
  return s
end

local function sub_argv (a, b, n)
  local s = 0
  for i = 1, n do s = s + a[i] - b[i] end
  return s
end

local function sum_mixed (a, b, n)
  local s = 0
  for i = 1, n do s = s + mixedA[i] + upB[i] end
  return s
end

local function sum_float (a, b, n)
  local s = 0.5
  for i = 1, n do s = s + a[i] + b[i] end
  return s
end

local function sub_float (a, b, n)
  local s = 1000.0
  for i = 1, n do s = s + a[i] - b[i] end
  return s
end

local function sumres (a, b, n)
  local s = 0
  local x
  for i = 1, n do
    x = a[i]
    s = s + x + b[i]
  end
  return s, x
end

local st = run(sum_up)
H.check(st.tint_hits >= 1, "upvalue sum did not use the pair intrinsic")
st = run(sub_up)
H.check(st.tint_hits >= 1, "upvalue difference did not use the pair intrinsic")
st = run(sum_argv, regA, regB)
H.check(st.tint_hits >= 1, "register sum did not use the pair intrinsic")
st = run(sub_argv, regA, regB)
H.check(st.tint_hits >= 1, "register difference did not use the pair intrinsic")
run(sum_mixed, regA, regB)
run(sum_float, mixedA, mixedB)
run(sub_float, mixedA, mixedB)
st = run(sumres, mixedA, mixedB)
H.check(st.tint_hits >= 1, "result-register sum did not use the pair intrinsic")

-- Two's-complement wrap and promotion are the interpreter's semantics.
run(sum_argv, bigA, bigB)
run(sub_argv, bigA, bigB)

-- Unsupported shapes must fall back without changing the observable result.
local short = {}
for i = 1, N - 1 do short[i] = i end
run(sum_argv, short, regB)

local withmeta = setmetatable({}, {
  __index = function (_, k) return k end,
})
for i = 1, N do rawset(withmeta, i, i) end
run(sum_argv, withmeta, regB)

local function stepped (a, b, n)
  local s = 0
  for i = 1, n, 2 do s = s + a[i] + b[i] end
  return s
end
run(stepped, regA, regB)

jit.flush()
jit.opt.start("hotloop=56", "ir=0", "table=0", "tint=0")
print("jit/165_pair_array_sum ok")
