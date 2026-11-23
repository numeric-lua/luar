-- jit/166_dot_product.lua -- native dot product and sum of squares.
--
-- `s = s + a[i] * b[i]` and `s = s + a[i] * a[i]` compile to one C pass
-- over two raw arrays, register or upvalue, with exact int-wrap and
-- float-promotion order.  The product temporary is also observable after the
-- loop, so the intrinsic has to preserve it.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/166_dot_product: skipped (no native target)")
  return
end

jit.opt.start("ir=0", "hotloop=1", "table=0", "tint=1")

local N = 300
local upA, upB = {}, {}
for i = 1, N do
  upA[i] = i
  upB[i] = i * 3
end

local regA, regB = {}, {}
for i = 1, N do
  regA[i] = i * 2 - 7
  regB[i] = i + 11
end

local mixedA, mixedB = {}, {}
for i = 1, N do
  mixedA[i] = (i % 17 == 0) and (i + 0.25) or i
  mixedB[i] = (i % 13 == 0) and (i - 0.5) or i * 2
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
    H.eq(s1, s2, "result")
    H.eq(x1, x2, "product register")
  else
    H.eq(tostring(s1), tostring(s2), "error message")
  end
  return st
end

local function dot_up (a, b, n)
  local s = 0
  for i = 1, n do s = s + upA[i] * upB[i] end
  return s
end

local function dot_argv (a, b, n)
  local s = 0
  for i = 1, n do s = s + a[i] * b[i] end
  return s
end

local function dot_float (a, b, n)
  local s = 1.5
  for i = 1, n do s = s + a[i] * b[i] end
  return s
end

local function square (a, b, n)
  local s = 0
  for i = 1, n do s = s + a[i] * a[i] end
  return s
end

local function square_float (a, b, n)
  local s = 0.25
  for i = 1, n do s = s + a[i] * a[i] end
  return s
end

local function dotres (a, b, n)
  local s = 0
  local x
  for i = 1, n do
    x = a[i] * b[i]
    s = s + x
  end
  return s, x
end

local st = run(dot_up)
H.check(st.tint_hits >= 1, "upvalue dot product did not use the intrinsic")
st = run(dot_argv, regA, regB)
H.check(st.tint_hits >= 1, "register dot product did not use the intrinsic")
run(dot_float, regA, regB)
run(dot_argv, mixedA, mixedB)
st = run(square, regA, regB)
H.check(st.tint_hits >= 1, "sum of squares did not use the intrinsic")
run(square_float, mixedA, mixedB)
st = run(dotres, regA, regB)
H.check(st.tint_hits >= 1, "result-register dot product did not use the intrinsic")

-- Wrap and middle-of-run float promotion match the interpreter.
run(dot_argv, bigA, bigB)
run(dot_argv, mixedA, bigB)

-- Unsupported shapes fall back to the interpreter without changing values.
local short = {}
for i = 1, N - 1 do short[i] = i end
run(dot_argv, short, regB)

local withmeta = setmetatable({}, { __index = function (_, k) return k end })
for i = 1, N do rawset(withmeta, i, i) end
run(dot_argv, withmeta, regB)

local function stepped (a, b, n)
  local s = 0
  for i = 1, n, 2 do s = s + a[i] * b[i] end
  return s
end
run(stepped, regA, regB)

jit.flush()
jit.opt.start("hotloop=56", "ir=0", "table=0", "tint=0")
print("jit/166_dot_product ok")
