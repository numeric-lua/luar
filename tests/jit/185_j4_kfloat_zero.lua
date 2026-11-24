-- jit/185_j4_kfloat_zero.lua -- a float K constant is not an integer K
-- constant just because its value compares equal to zero.
--
-- The recorder used nval != ival to classify a K operand, so a float 0.0
-- was emitted with integer ADDK/SUBK/MULK semantics and the result was
-- retagged as an integer.  An explicit K-float tag now keeps the trace on
-- the interpreter when no faithful float form exists.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/185_j4_kfloat_zero: skipped (no native target)")
  return
end

local function bits (x) return string.pack("d", x) end
local function same (a, b)
  if math.type(a) ~= math.type(b) then return false end
  if math.type(a) == "float" then
    if a ~= a and b ~= b then return true end
    return bits(a) == bits(b)
  end
  return a == b
end

jit.flush()
jit.opt.start("ir=1", "hotloop=1", "hotexit=1", "table=0", "tint=0")
jit.on()

local function loadf (src)
  return assert(load(src, "=j4kfloat"))()
end

local f = loadf([[
  return function (n)
    local x = 9007199254740993
    local u = 0
    for i = n, 1, -1 do u = x + 0.0 end
    return u, math.type(u)
  end
]])
local u, ut = f(10)
H.check(same(u, 9007199254740992.0), "x + 0.0 result")
H.eq(ut, "float", "x + 0.0 must produce a float")

local fsub = loadf([[
  return function (n)
    local x = 5
    local u = 0
    for i = n, 1, -1 do u = x - 0.0 end
    return u, math.type(u)
  end
]])
u, ut = fsub(10)
H.check(same(u, 5.0), "x - 0.0 result")
H.eq(ut, "float", "x - 0.0 must produce a float")

local fmul = loadf([[
  return function (n)
    local x = 5
    local u = 0
    for i = n, 1, -1 do u = x * 0.0 end
    return u, math.type(u)
  end
]])
u, ut = fmul(10)
H.check(same(u, 0.0), "x * 0.0 result")
H.eq(ut, "float", "x * 0.0 must produce a float")

-- DIVK is always a float operation, even with an integer constant.
local fdiv = loadf([[
  return function (n)
    local x = 5
    local u = 0
    for i = n, 1, -1 do u = x / 2 end
    return u, math.type(u)
  end
]])
u, ut = fdiv(10)
H.check(same(u, 2.5), "x / 2 result")
H.eq(ut, "float", "x / 2 must produce a float")

jit.flush()
jit.opt.start("ir=0", "hotloop=56", "hotexit=10", "table=1", "tint=1")
jit.on()
print("jit/185_j4_kfloat_zero ok")
