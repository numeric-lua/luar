-- jit/167_three_sum_upvalues.lua -- the three-array sum intrinsic accepts
-- closure upvalues as well as register tables.
--
-- `s = s + a[i] + b[i] + c[i]` is the common three-array reduction shape.
-- The original intrinsic only matched three register tables; when any
-- source was a closure upvalue the compiler emitted GETUPVAL before each
-- GETTABLE and the loop stayed interpreted.  Every source is now encoded
-- independently (register or upvalue), re-resolved at each entry, and the
-- intrinsic keeps its exact int-wrap and float-promotion order.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/167_three_sum_upvalues: skipped (no native target)")
  return
end

local function bits (x)
  return string.pack("d", x)
end

local function same (a, b)
  if math.type(a) ~= math.type(b) then return false end
  if math.type(a) == "float" then
    if a ~= a and b ~= b then return true end
    return bits(a) == bits(b)
  end
  return a == b
end

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "table=0", "tint=1")

local N = 300
local upA, upB, upC = {}, {}, {}
for i = 1, N do
  upA[i] = i * 3
  upB[i] = i - 100
  upC[i] = i * 2 + 1
end

local mixedA, mixedB, mixedC = {}, {}, {}
for i = 1, N do
  mixedA[i] = (i % 17 == 0) and (i + 0.25) or i
  mixedB[i] = (i % 13 == 0) and (i - 0.5) or i * 2
  mixedC[i] = (i % 19 == 0) and (i + 0.75) or i * 3
end

local bigA, bigB, bigC = {}, {}, {}
for i = 1, N do
  bigA[i] = math.maxinteger - i
  bigB[i] = i * 1000003
  bigC[i] = -i * 999983
end

local function run (f, a, b, c)
  jit.flush()
  jit.off()
  local ok1, s1 = pcall(f, a, b, c, N)
  jit.flush()
  jit.on()
  local before = jit.stats().tint_hits
  local ok2, s2 = pcall(f, a, b, c, N)
  local st = jit.stats()
  H.eq(ok1, ok2, "error status")
  if ok1 then
    H.check(same(s1, s2), "result differs: " .. tostring(s1) .. " ~= " ..
                            tostring(s2))
  else
    H.eq(tostring(s1), tostring(s2), "error message")
  end
  return st, before
end

-- All three sources are closure upvalues.
local function sum_up (a, b, c, n)
  local s = 0
  for i = 1, n do s = s + upA[i] + upB[i] + upC[i] end
  return s
end

-- One source is a register local, two are upvalues.  `x` makes a local
-- copy so the bytecode reads it with GETTABLE, not GETUPVAL.
local function sum_mixed (a, b, c, n)
  local x = upA
  local s = 0
  for i = 1, n do s = s + x[i] + upB[i] + upC[i] end
  return s
end

-- The same upvalue array read three times (the compiler may reuse the
-- second and third element registers).
local function sum_same (a, b, c, n)
  local s = 0
  for i = 1, n do s = s + upA[i] + upA[i] + upA[i] end
  return s
end

-- Register tables keep the original path and stay covered.
local function sum_reg (a, b, c, n)
  local s = 0
  for i = 1, n do s = s + a[i] + b[i] + c[i] end
  return s
end

local function sum_float (a, b, c, n)
  local s = 0.5
  for i = 1, n do s = s + a[i] + b[i] + c[i] end
  return s
end

local function sum_mixed_elem (a, b, c, n)
  local s = 0
  for i = 1, n do s = s + a[i] + b[i] + c[i] end
  return s
end

local st, before = run(sum_up, upA, upB, upC)
H.check(st.tint_hits > before, "upvalue three-array sum did not use the intrinsic")
H.check(st.compiled >= 1, "upvalue three-array loop did not compile")

st, before = run(sum_mixed, upA, upB, upC)
H.check(st.tint_hits > before, "mixed register/upvalue sum did not use the intrinsic")

st, before = run(sum_same, upA, upB, upC)
H.check(st.tint_hits > before, "repeated upvalue sum did not use the intrinsic")

st, before = run(sum_reg, upA, upB, upC)
H.check(st.tint_hits > before, "register three-array sum regressed")

run(sum_float, upA, upB, upC)
run(sum_mixed_elem, mixedA, mixedB, mixedC)
run(sum_up, bigA, bigB, bigC)
run(sum_mixed, mixedA, bigB, bigC)

-- A reassigned upvalue is observed by the next entry.  The compiled loop
-- must reload it rather than caching the old table.
local savedA = upA
upA = mixedA
run(sum_up, upA, upB, upC)
upA = savedA

-- Short arrays, metatables and non-numeric elements fall back without
-- changing the visible accumulator (or the error).
local shortC = {}
for i = 1, N - 1 do shortC[i] = i end
run(sum_up, upA, upB, shortC)

local metaA = setmetatable({}, { __index = function (_, k) return k end })
for i = 1, N do rawset(metaA, i, i) end
run(sum_up, metaA, upB, upC)

local badC = {}
for i = 1, N do badC[i] = i end
badC[N // 2] = "not a number"
run(sum_up, upA, upB, badC)

-- Non-unit steps use the same key walk as FORLOOP.
local function stepped (a, b, c, n)
  local s = 0
  for i = 1, n, 2 do s = s + upA[i] + upB[i] + upC[i] end
  return s
end
local st, before = run(stepped, upA, upB, upC)
H.check(st.tint_hits > before, "strided upvalue sum did not use the intrinsic")

jit.flush()
jit.opt.start("hotloop=56", "ir=0", "table=0", "tint=0")
print("jit/167_three_sum_upvalues ok")
