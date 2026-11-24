-- jit/192_tint_writeback.lua -- native intrinsics must leave every visible
-- bytecode temporary with its interpreter value.
--
-- The three-table sum wrote only the accumulator and the third element;
-- the intermediate `sumreg` and the second element stayed stale.  The
-- keyed fill wrote the table but not the key temporary read from the key
-- table.  Both are ordinary locals that source code can observe after the
-- loop.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/192_tint_writeback: skipped (no native target)")
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

local N = 120
local t, u, v = {}, {}, {}
for i = 1, N do
  t[i] = i
  u[i] = 2 * i
  v[i] = 3 * i
end

-- Three-table sum: `b` is the intermediate register and `c` the second
-- element; both must hold their last-iteration values after the loop.
local function three (t, u, v, n)
  local s = 0
  local b, c
  for i = 1, n do
    b = t[i]
    b = s + b
    c = u[i]
    b = b + c
    s = b + v[i]
  end
  return s, b, c
end

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.off()
local ws, wb, wc = three(t, u, v, N)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
local gs, gb, gc = three(t, u, v, N)
H.check(jit.stats().tint_hits > before, "three-table sum intrinsic did not run")
H.check(same(gs, ws), "three-table sum result")
H.check(same(gb, wb), "three-table sum intermediate")
H.check(same(gc, wc), "three-table sum second element")

-- Aliased second and third element registers: the later third-element
-- write must win, as in the bytecode.
local function three_alias (t, u, v, n)
  local s = 0
  local b, c
  for i = 1, n do
    b = t[i]
    b = s + b
    c = u[i]
    b = b + c
    c = v[i]
    s = b + c
  end
  return s, b, c
end

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.off()
local was, wab, wac = three_alias(t, u, v, N)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
local gas, gab, gac = three_alias(t, u, v, N)
H.check(jit.stats().tint_hits > before, "aliased three-table sum did not run")
H.check(same(gas, was), "aliased three-table sum result")
H.check(same(gab, wab), "aliased three-table sum intermediate")
H.check(same(gac, wac), "aliased three-table sum third element")

-- Keyed fill: the key temporary `x` must hold the last key after the loop.
local keys = {}
for i = 1, N do keys[i] = "key:" .. i end

local function keyed_fill (keys, t, n)
  local x
  for i = 1, n do
    x = keys[i]
    t[x] = i
  end
  return x, t
end

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.off()
local wx, wt = keyed_fill(keys, {}, N)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
local gx, gt = keyed_fill(keys, {}, N)
H.check(jit.stats().tint_hits > before, "keyed fill intrinsic did not run")
H.eq(gx, wx, "keyed fill key temporary")
for i = 1, N do
  H.eq(gt["key:" .. i], wt["key:" .. i], "keyed fill element " .. i)
end

-- Keyed fill through an upvalue key table keeps the same temporary.
local ukeys = keys
local function up_keyed_fill (t, n)
  local x
  for i = 1, n do
    x = ukeys[i]
    t[x] = i
  end
  return x, t
end

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.off()
local ux, ut = up_keyed_fill({}, N)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
local ux2, ut2 = up_keyed_fill({}, N)
H.check(jit.stats().tint_hits > before, "upvalue keyed fill did not run")
H.eq(ux2, ux, "upvalue keyed fill key temporary")
for i = 1, N do
  H.eq(ut2["key:" .. i], ut["key:" .. i], "upvalue keyed fill element " .. i)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/192_tint_writeback ok")
