-- ffi/178_j2_cdata_pair_idiv.lua -- J2 two-source cdata floor division
-- and modulo: `s = s +|- a[i] // b[i]` and `a[i] % b[i]`.
--
-- Integer elements use Lua's floor rules and int wrap; a zero integer
-- divisor must raise through the interpreter, while floating elements use
-- the non-raising floor/fmod path.  The source temporaries and accumulator
-- are compared with the interpreter.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/178_j2_cdata_pair_idiv: skipped (no native target)")
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

local N = 180
local ai = ffi.new("int32_t[?]", N)
local bi = ffi.new("int32_t[?]", N)
local af = ffi.new("double[?]", N)
local bf = ffi.new("double[?]", N)
for i = 0, N - 1 do
  ai[i] = i * 7 - 300
  bi[i] = i + 3
  af[i] = i - 40 + 0.5
  bf[i] = i + 2.5
end
local UA, UB = af, bf

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
  for i = 0, n - 1 do s = s + a[i] // b[i] end
  return s
end

local function mod_add (a, b, n)
  local s = 0
  for i = 0, n - 1 do s = s + a[i] % b[i] end
  return s
end

local function idiv_sub (a, b, n)
  local s = 0
  for i = 0, n - 1 do s = s - a[i] // b[i] end
  return s
end

local function mod_sub (a, b, n)
  local s = 0
  for i = 0, n - 1 do s = s - a[i] % b[i] end
  return s
end

local function float_idiv (a, b, n)
  local s = 0.0
  for i = 0, n - 1 do s = s + a[i] // b[i] end
  return s
end

local function float_mod (a, b, n)
  local s = 0.0
  for i = 0, n - 1 do s = s + a[i] % b[i] end
  return s
end

local function up_idiv (a, b, n)
  local s = 0
  for i = 0, n - 1 do s = s + UA[i] // UB[i] end
  return s
end

local function down_mod (a, b, n)
  local s = 0
  for i = n - 1, 0, -1 do s = s + a[i] % b[i] end
  return s
end

local function step_idiv (a, b, n)
  local s = 0
  for i = 0, n - 1, 3 do s = s + a[i] // b[i] end
  return s
end

local function observe (a, b, n)
  local s = 0
  local q
  for i = 0, n - 1 do
    q = a[i] // b[i]
    s = s + q
  end
  return s, q
end

run("int floor add", idiv_add, ai, bi, N)
run("int mod add", mod_add, ai, bi, N)
run("int floor sub", idiv_sub, ai, bi, N)
run("int mod sub", mod_sub, ai, bi, N)
run("float floor", float_idiv, af, bf, N)
run("float mod", float_mod, af, bf, N)
run("upvalue floor", up_idiv, af, bf, N)
run("descending mod", down_mod, ai, bi, N)
run("strided floor", step_idiv, ai, bi, N)
run("observed temporary", observe, af, bf, N)

-- Integer zero divisor raises and does not run the intrinsic.
local zi = ffi.new("int32_t[?]", N)
local zf = ffi.new("double[?]", N)
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
local ok = pcall(function ()
  local s = 0
  for i = 0, N - 1 do s = s + ai[i] // zi[i] end
  return s
end)
H.check(not ok, "integer floor division by zero must raise")
H.eq(jit.stats().tint_hits, before, "zero divisor must not use the intrinsic")

-- Float zero follows the non-raising path.
run("float zero floor", float_idiv, af, zf, N)
run("float zero mod", float_mod, af, zf, N)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/178_j2_cdata_pair_idiv ok")
