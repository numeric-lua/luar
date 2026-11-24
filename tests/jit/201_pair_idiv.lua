-- jit/201_pair_idiv.lua -- native `s = s +|- a[i] // b[i]` and
-- `a[i] % b[i]` reductions over raw table arrays.
--
-- Floor division and modulo follow Lua's negative-operand floor rules and
-- float promotion.  An integer zero divisor must raise in the interpreter;
-- a float operand takes the non-raising float path.  The helper rejects an
-- all-integer zero divisor before writing the accumulator or temporaries.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/201_pair_idiv: skipped (no native target)")
  return
end

local function same (a, b)
  if math.type(a) ~= math.type(b) then return false end
  if math.type(a) == "float" then
    if a ~= a and b ~= b then return true end
    return string.pack("d", a) == string.pack("d", b)
  end
  return a == b
end

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "hotexit=1", "tint=1")
jit.on()

local N = 240
local A, B, F, G = {}, {}, {}, {}
for i = 1, N do
  A[i] = i * 3 - 50
  B[i] = i + 3
  F[i] = i - 40 + 0.5
  G[i] = i + 2.5
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
              name .. ": result " .. i .. " " .. tostring(got[i]) ..
              " ~= " .. tostring(want[i]))
    end
  else
    H.eq(tostring(got[2]), tostring(want[2]), name .. ": error message")
  end
  H.check(used > 0, name .. ": intrinsic did not run")
end

local function idiv_add (a, b, n)
  local s = 0
  for i = 1, n do s = s + a[i] // b[i] end
  return s
end

local function mod_add (a, b, n)
  local s = 0
  for i = 1, n do s = s + a[i] % b[i] end
  return s
end

local function idiv_sub (a, b, n)
  local s = 0
  for i = 1, n do s = s - a[i] // b[i] end
  return s
end

local function mod_sub (a, b, n)
  local s = 0
  for i = 1, n do s = s - a[i] % b[i] end
  return s
end

local function float_idiv (a, b, n)
  local s = 0.0
  for i = 1, n do s = s + a[i] // b[i] end
  return s
end

local function float_mod (a, b, n)
  local s = 0.0
  for i = 1, n do s = s + a[i] % b[i] end
  return s
end

local function up_idiv (a, b, n)
  local s = 0
  for i = 1, n do s = s + UA[i] // UB[i] end
  return s
end

local function down_mod (a, b, n)
  local s = 0
  for i = n, 1, -1 do s = s + a[i] % b[i] end
  return s
end

local function step_idiv (a, b, n)
  local s = 0
  for i = 1, n, 3 do s = s + a[i] // b[i] end
  return s
end

local function observe (a, b, n)
  local s = 0
  local q
  for i = 1, n do
    q = a[i] // b[i]
    s = s + q
  end
  return s, q
end

run("floor add", idiv_add, A, B, N)
run("mod add", mod_add, A, B, N)
run("floor sub", idiv_sub, A, B, N)
run("mod sub", mod_sub, A, B, N)
run("float floor", float_idiv, F, G, N)
run("float mod", float_mod, F, G, N)
run("upvalue sources", up_idiv, A, B, N)
run("descending mod", down_mod, A, B, N)
run("strided floor", step_idiv, A, B, N)
run("observed temporaries", observe, A, B, N)

-- Integer zero divisor must raise and not run the intrinsic.
local Z = {}
for i = 1, N do Z[i] = 0 end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
local ok = pcall(function ()
  local s = 0
  for i = 1, N do s = s + A[i] // Z[i] end
  return s
end)
H.check(not ok, "integer floor division by zero must raise")
H.eq(jit.stats().tint_hits, before, "zero divisor must not use the intrinsic")

-- Float zero is the non-raising float path.
local FZ = {}
for i = 1, N do FZ[i] = 0.0 end
run("float zero floor", float_idiv, F, FZ, N)
run("float zero mod", float_mod, F, FZ, N)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/201_pair_idiv ok")
