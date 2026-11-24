-- jit/204_table_pair_store.lua -- native `dst[i] = a[i] OP b[i]` stores
-- over raw table arrays.
--
-- The two source arrays may be registers or closure upvalues; the
-- destination may be one of them.  Add/subtract/multiply/divide and
-- floor-division/modulo follow Lua's int-wrap and float-promotion rules.
-- An integer zero divisor declines before any store so the interpreter
-- raises; a float zero follows the non-raising path.  The second-element
-- and result registers keep their last bytecode values.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/204_table_pair_store: skipped (no native target)")
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
  A[i] = i - 100
  B[i] = i + 3
  F[i] = i - 100 + 0.5
  G[i] = i + 2.5
end
local UA, UB = F, G

local function run (name, f, a, b, ...)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local want = f({}, a, b, ...)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = f({}, a, b, ...)
  local used = jit.stats().tint_hits - before
  H.check(used > 0, name .. ": intrinsic did not run")
  for i = 1, N do
    H.check(same(got[i], want[i]),
            name .. ": element " .. i .. " " .. tostring(got[i]) ..
            " ~= " .. tostring(want[i]))
    H.eq(math.type(got[i]), math.type(want[i]), name .. ": type " .. i)
  end
end

local function op_add (d, a, b, n) for i = 1, n do d[i] = a[i] + b[i] end return d end
local function op_sub (d, a, b, n) for i = 1, n do d[i] = a[i] - b[i] end return d end
local function op_mul (d, a, b, n) for i = 1, n do d[i] = a[i] * b[i] end return d end
local function op_div (d, a, b, n) for i = 1, n do d[i] = a[i] / b[i] end return d end
local function op_idiv (d, a, b, n) for i = 1, n do d[i] = a[i] // b[i] end return d end
local function op_mod (d, a, b, n) for i = 1, n do d[i] = a[i] % b[i] end return d end

run("add", op_add, A, B, N)
run("subtract", op_sub, A, B, N)
run("multiply", op_mul, A, B, N)
run("divide", op_div, A, B, N)
run("floor divide", op_idiv, A, B, N)
run("modulo", op_mod, A, B, N)
run("float add", op_add, F, G, N)
run("float divide", op_div, F, G, N)

local function up_add (d, a, b, n)
  for i = 1, n do d[i] = UA[i] + UB[i] end
  return d
end
run("upvalue sources", up_add, F, G, N)

-- In-place store: destination is the first source.
local function inplace (a, b, n)
  for i = 1, n do a[i] = a[i] + b[i] end
  return a
end
local p1, p2 = {}, {}
for i = 1, N do p1[i] = A[i]; p2[i] = A[i] end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.off()
inplace(p1, B, N)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
inplace(p2, B, N)
H.check(jit.stats().tint_hits > before, "in-place store did not run")
for i = 1, N do H.check(same(p2[i], p1[i]), "in-place element " .. i) end

-- Observed second element and result temporaries.
local function observe (d, a, b, n)
  local x, y
  for i = 1, n do
    x = a[i]
    y = b[i]
    d[i] = x * y
  end
  return d, x, y
end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.off()
local wd, wx, wy = observe({}, A, B, N)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
local gd, gx, gy = observe({}, A, B, N)
H.check(jit.stats().tint_hits > before, "observed store did not run")
H.check(same(gx, wx), "observed result")
H.check(same(gy, wy), "observed second element")
for i = 1, N do H.check(same(gd[i], wd[i]), "observed element " .. i) end

-- Integer zero divisor declines; float zero does not raise.
local Z = {}
for i = 1, N do Z[i] = 0 end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
local ok = pcall(function ()
  local d = {}
  for i = 1, N do d[i] = A[i] // Z[i] end
  return d
end)
H.check(not ok, "integer zero divisor must raise")
H.eq(jit.stats().tint_hits, before, "zero divisor must not use the intrinsic")
run("float zero divisor", op_div, F, Z, N)

-- Destination metatable declines.
local seen = 0
local meta = setmetatable({}, {
  __newindex = function (t, k, v) seen = seen + 1; rawset(t, k, v) end,
})
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
for i = 1, N do meta[i] = A[i] + B[i] end
H.eq(jit.stats().tint_hits, before, "metatable destination must not use the intrinsic")
H.eq(seen, N, "__newindex call count")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/204_table_pair_store ok")
