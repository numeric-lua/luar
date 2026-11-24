-- jit/200_table_idiv_store.lua -- native `dst[i] = src[i] // k` and
-- `dst[i] = src[i] % k` over raw table arrays.
--
-- Floor division and modulo follow Lua's negative-operand floor rules,
-- integer wrap and float promotion.  An integer zero divisor must raise in
-- the interpreter, but a float source takes the non-raising float path, so
-- the helper pre-checks the source element tags before storing anything.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/200_table_idiv_store: skipped (no native target)")
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

local N = 200
local I, F = {}, {}
for i = 1, N do
  I[i] = i - 100
  F[i] = i - 100 + 0.5
end
local K = 7
local UI = I

local function run (name, f, src, ...)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.off()
  local want = f({}, src, ...)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = f({}, src, ...)
  local used = jit.stats().tint_hits - before
  H.check(used > 0, name .. ": intrinsic did not run")
  for i = 1, N do
    H.check(same(got[i], want[i]),
            name .. ": element " .. i .. " " .. tostring(got[i]) ..
            " ~= " .. tostring(want[i]))
    H.eq(math.type(got[i]), math.type(want[i]), name .. ": type " .. i)
  end
end

local function idiv_const (dst, src, n)
  for i = 1, n do dst[i] = src[i] // 3 end
  return dst
end

local function mod_const (dst, src, n)
  for i = 1, n do dst[i] = src[i] % 3 end
  return dst
end

local function idiv_reg (dst, src, n, k)
  local d = k
  for i = 1, n do dst[i] = src[i] // d end
  return dst
end

local function mod_reg (dst, src, n, k)
  local d = k
  for i = 1, n do dst[i] = src[i] % d end
  return dst
end

local function idiv_up_scalar (dst, src, n)
  for i = 1, n do dst[i] = src[i] // K end
  return dst
end

local function mod_up_scalar (dst, src, n)
  for i = 1, n do dst[i] = src[i] % K end
  return dst
end

local function idiv_up_src (dst, src, n)
  for i = 1, n do dst[i] = UI[i] // 4 end
  return dst
end

local function idiv_down (dst, src, n)
  for i = n, 1, -1 do dst[i] = src[i] // 4 end
  return dst
end

local function mod_step (dst, src, n)
  for i = 1, n, 3 do dst[i] = src[i] % 4 end
  return dst
end

run("floor division constant", idiv_const, I, N)
run("modulo constant", mod_const, I, N)
run("floor division register", idiv_reg, I, N, -4)
run("modulo register", mod_reg, I, N, -4)
run("floor division upvalue scalar", idiv_up_scalar, I, N)
run("modulo upvalue scalar", mod_up_scalar, I, N)
run("floor division upvalue source", idiv_up_src, I, N)
run("floor division descending", idiv_down, I, N)
run("modulo strided", mod_step, I, N)
run("float floor division", idiv_reg, F, N, 7)
run("float modulo", mod_reg, F, N, 7)

-- An integer zero divisor with integer elements must fall back and raise.
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local ok = pcall(function ()
  local d = {}
  for i = 1, N do d[i] = I[i] // 0 end
  return d
end)
H.check(not ok, "integer zero divisor must raise")
ok = pcall(function ()
  local d = {}
  for i = 1, N do d[i] = I[i] % 0 end
  return d
end)
H.check(not ok, "integer zero modulo must raise")

-- A float source with an integer zero scalar follows the float path.
run("float source zero divisor", idiv_reg, F, N, 0)
run("float source zero modulo", mod_reg, F, N, 0)

-- Mixed integer/float source with an integer zero divisor still raises at
-- the first integer element, so the whole run falls back.
local mixed = {}
for i = 1, N do
  mixed[i] = (i % 2 == 0) and (i + 0.5) or i
end
ok = pcall(function ()
  local d = {}
  for i = 1, N do d[i] = mixed[i] // 0 end
  return d
end)
H.check(not ok, "mixed source zero divisor must raise")

-- Destination metatable keeps __newindex in the interpreter.
local seen = 0
local meta = setmetatable({}, {
  __newindex = function (t, k, v) seen = seen + 1; rawset(t, k, v) end,
})
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
for i = 1, N do meta[i] = I[i] // 2 end
H.eq(jit.stats().tint_hits, before, "metatable destination must not use the intrinsic")
H.eq(seen, N, "__newindex call count")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/200_table_idiv_store ok")
