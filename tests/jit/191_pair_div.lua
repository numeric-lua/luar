-- jit/191_pair_div.lua -- native `s = s + a[i] / b[i]` reductions.
--
-- The two-array intrinsic covered add, subtract, multiply and dot product
-- but not division.  Division always produces a float and is not
-- commutative, so only the left-to-right operand order is accepted.  Both
-- sources may be registers or closure upvalues.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/191_pair_div: skipped (no native target)")
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
jit.opt.start("ir=0", "hotloop=1", "hotexit=1", "tint=1")
jit.on()

local N = 300
local A, B, F, G = {}, {}, {}, {}
for i = 1, N do
  A[i] = i * 3 - 40
  B[i] = i + 1
  F[i] = i * 0.5 + 0.25
  G[i] = (i % 7 == 0) and 0.0 or (i - 0.5)
end
local UA, UB = A, B

local function run (name, f, a, b, ...)
  jit.flush()
  jit.off()
  local want = { pcall(f, a, b, ...) }
  jit.flush()
  jit.on()
  local before = jit.stats().tint_hits
  local got = { pcall(f, a, b, ...) }
  local used = jit.stats().tint_hits - before
  H.eq(got[1], want[1], name .. ": error status")
  H.eq(#got, #want, name .. ": result count")
  if got[1] then
    for i = 2, #want do
      H.check(same(got[i], want[i]),
              name .. ": result " .. i .. " differs: " ..
              tostring(got[i]) .. " ~= " .. tostring(want[i]))
    end
  else
    H.eq(tostring(got[2]), tostring(want[2]), name .. ": error message")
  end
  H.check(used > 0, name .. ": intrinsic did not run")
end

local function div_int (a, b, n)
  local s = 0
  for i = 1, n do s = s + a[i] / b[i] end
  return s
end

local function div_float (a, b, n)
  local s = 0.0
  for i = 1, n do s = s + a[i] / b[i] end
  return s
end

local function div_up (a, b, n)
  local s = 0
  for i = 1, n do s = s + UA[i] / UB[i] end
  return s
end

local function div_zero (a, b, n)
  local s = 0
  for i = 1, n do s = s + a[i] / b[i] end
  return s
end

local function div_down (a, b, n)
  local s = 0
  for i = n, 1, -1 do s = s + a[i] / b[i] end
  return s
end

local function div_step (a, b, n)
  local s = 0
  for i = 1, n, 3 do s = s + a[i] / b[i] end
  return s
end

local function observe (a, b, n)
  local s = 0
  local x, y, q
  for i = 1, n do
    x = a[i]
    y = b[i]
    q = x / y
    s = s + q
  end
  return s, x, y, q
end

local function mul_still (a, b, n)
  local s = 0
  for i = 1, n do s = s + a[i] * b[i] end
  return s
end

run("integer division", div_int, A, B, N)
run("float division", div_float, F, G, N)
run("upvalue arrays", div_up, A, B, N)
run("zero denominators", div_zero, F, G, N)
run("descending division", div_down, A, B, N)
run("strided division", div_step, A, B, N)
run("observed temporaries", observe, A, B, N)
run("multiply unchanged", mul_still, A, B, N)

-- `b[i] / a[i]` reads b first and a second, so the intrinsic must divide
-- the first read by the second and stay correct for this order too.
local function reversed (a, b, n)
  local s = 0
  for i = 1, n do s = s + b[i] / a[i] end
  return s
end
jit.flush()
jit.off()
local rwant = reversed(A, B, N)
jit.flush()
jit.on()
local before = jit.stats().tint_hits
local rgot = reversed(A, B, N)
H.check(same(rgot, rwant), "reversed division result")
H.check(jit.stats().tint_hits > before, "reversed division did not use the intrinsic")

-- A metatable source must fall back and keep the interpreter's error.
local meta = setmetatable({}, { __index = function () return "x" end })
jit.flush()
jit.on()
local ok = pcall(function ()
  local s = 0
  for i = 1, N do s = s + meta[i] / B[i] end
  return s
end)
H.check(not ok, "metatable source must raise")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/191_pair_div ok")
