-- jit/174_leaf_arg_variation.lua -- leaf-call fast paths may only snapshot
-- arguments that are loop-invariant.
--
-- The leaf2/3/4 helpers read their extra argument registers once, before
-- running the remaining iterations.  A shape such as `s = s + f(i, i)` used
-- the first index for every iteration when compiled, while the interpreter
-- re-reads the argument slot on each call.  Such loops must fall back to the
-- interpreter; a genuinely constant argument still compiles.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/174_leaf_arg_variation: skipped (no native target)")
  return
end

local function mul (a, b) return a * b end
local function add (a, b) return a + b end
local function mul3 (a, b, c) return a * b * c end
local function add4 (a, b, c, d) return a + b + c + d end

local function sum_ii (f, n)
  local s = 0
  for i = 1, n do s = s + f(i, i) end
  return s
end

local function sum_is_mul (f, n)
  local s = 0
  for i = 1, n do s = s + f(i, s) end
  return s
end

local function sum_is_add (f, n)
  local s = 0
  for i = 1, n do s = s + f(i, s) end
  return s
end

local function sum_iii (f, n)
  local s = 0
  for i = 1, n do s = s + f(i, i, i) end
  return s
end

local function sum_iiii (f, n)
  local s = 0
  for i = 1, n do s = s + f(i, i, i, i) end
  return s
end

local function sum_ib (f, n, b)
  local s = 0
  for i = 1, n do s = s + f(i, b) end
  return s
end

local function run (name, f, ...)
  local args = { ... }
  jit.flush()
  jit.off()
  local want = f(table.unpack(args))
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "table=0", "tint=1")
  jit.on()
  f(table.unpack(args))
  local got = f(table.unpack(args))
  H.eq(got, want, name .. ": compiled result differs from interpreter")
end

-- Varying extra arguments: these must not use the snapshotting helpers.
run("index argument", sum_ii, mul, 200)
run("accumulator argument (mul)", sum_is_mul, mul, 60)
run("accumulator argument (add)", sum_is_add, add, 100)
run("third index argument", sum_iii, mul3, 40)
run("four index arguments", sum_iiii, add4, 30)

-- A loop-invariant argument still takes the fast path.
jit.flush()
jit.off()
local want = sum_ib(mul, 1000, 3)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "table=0", "tint=1")
jit.on()
local before = jit.stats().tint_hits
local got = sum_ib(mul, 1000, 3)
H.eq(got, want, "constant leaf argument regressed")
H.check(jit.stats().compiled >= 1 and jit.stats().tint_hits > before,
        "constant leaf argument no longer compiles")

jit.flush()
jit.opt.start("ir=0", "hotloop=56")
print("jit/174_leaf_arg_variation ok")
