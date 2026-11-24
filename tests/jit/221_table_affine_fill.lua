-- jit/221_table_affine_fill.lua -- t[i] = i + k / i - k / i * k fills,
-- with register and closure-upvalue destinations.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/221_table_affine_fill: skipped (no native target)")
  return
end

local N = 200
local t = {}
local U = {}

local function run (name, f, want)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(N)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  for i = 1, N do H.eq(want.table[i], want.value(i), name .. " normal " .. i) end
end

local function add (n) for i = 1, n do t[i] = i + 2 end end
local function sub (n) for i = 1, n do t[i] = i - 2 end end
local function mul (n) for i = 1, n do t[i] = i * 3 end end
local function uadd (n) for i = 1, n do U[i] = i + 2 end end
local function usub (n) for i = 1, n do U[i] = i - 2 end end
local function umul (n) for i = 1, n do U[i] = i * 3 end end

run("add", add, { table = t, value = function (i) return i + 2 end })
run("sub", sub, { table = t, value = function (i) return i - 2 end })
run("mul", mul, { table = t, value = function (i) return i * 3 end })

local function run_up (name, f, value)
  U = {}
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(N)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  for i = 1, N do H.eq(U[i], value(i), name .. " upvalue " .. i) end
end

run_up("up add", uadd, function (i) return i + 2 end)
run_up("up sub", usub, function (i) return i - 2 end)
run_up("up mul", umul, function (i) return i * 3 end)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/221_table_affine_fill ok")

-- Strided and descending runs.
do
  t = {}
  local function stride (n) for i = 1, n, 3 do t[i] = i + 2 end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  stride(N)
  H.check(jit.stats().tint_hits > before, "strided fill did not run")
  for i = 1, N, 3 do H.eq(t[i], i + 2, "stride " .. i) end

  U = {}
  local function down (n) for i = n, 1, -2 do U[i] = i * 2 end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  before = jit.stats().tint_hits
  down(N)
  H.check(jit.stats().tint_hits > before, "descending fill did not run")
  for i = N, 1, -2 do H.eq(U[i], i * 2, "descending " .. i) end
end

-- Register and upvalue scalars.
do
  local K = 2
  t = {}
  local function reg_add (n) local k = 2; for i = 1, n do t[i] = i + k end end
  local function up_sub (n) for i = 1, n do t[i] = i - K end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  reg_add(N)
  H.check(jit.stats().tint_hits > before, "register scalar fill did not run")
  for i = 1, N do H.eq(t[i], i + 2, "register scalar " .. i) end
  before = jit.stats().tint_hits
  up_sub(N)
  H.check(jit.stats().tint_hits > before, "upvalue scalar fill did not run")
  for i = 1, N do H.eq(t[i], i - 2, "upvalue scalar " .. i) end
end

-- Float scalars promote each result to float.
do
  local F = 2.5
  t = {}
  local function flt_add (n) for i = 1, n do t[i] = i + F end end
  local function flt_mul (n) local k = 2.5; for i = 1, n do t[i] = i * k end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  flt_add(N)
  H.check(jit.stats().tint_hits > before, "float upvalue scalar did not run")
  for i = 1, N do
    H.eq(t[i], i + 2.5, "float add " .. i)
    H.eq(math.type(t[i]), "float", "float type " .. i)
  end
  before = jit.stats().tint_hits
  flt_mul(N)
  H.check(jit.stats().tint_hits > before, "float register scalar did not run")
  for i = 1, N do H.eq(t[i], i * 2.5, "float mul " .. i) end
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/221_table_affine_fill extended ok")
