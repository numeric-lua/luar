-- ffi/196_j2_i64_update.lua -- J2 in-place 8-byte cdata updates with a
-- cdata scalar.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/196_j2_i64_update: skipped (no native target)")
  return
end

local N = 200
local function init ()
  local a = ffi.new("int64_t[?]", N)
  for i = 0, N - 1 do a[i] = i + 1 end
  return a
end

local function run (name, f, scalar)
  jit.off()
  local want = f(init(), scalar, N)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local a = init()
  local before = jit.stats().tint_hits
  f(a, scalar, N)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  for i = 0, N - 1 do H.eq(a[i], want[i], name .. ": element " .. i) end
end

local function add (a, v, n) for i = 0, n - 1 do a[i] = a[i] + v end return a end
local function sub (a, v, n) for i = 0, n - 1 do a[i] = a[i] - v end return a end
local function mul (a, v, n) for i = 0, n - 1 do a[i] = a[i] * v end return a end
local function band (a, v, n) for i = 0, n - 1 do a[i] = a[i] & v end return a end
local function bor (a, v, n) for i = 0, n - 1 do a[i] = a[i] | v end return a end
local function bxor (a, v, n) for i = 0, n - 1 do a[i] = a[i] ~ v end return a end
local function shl (a, v, n) for i = 0, n - 1 do a[i] = a[i] << v end return a end
local function shr (a, v, n) for i = 0, n - 1 do a[i] = a[i] >> v end return a end

local function idiv (a, v, n) for i = 0, n - 1 do a[i] = a[i] // v end return a end
local function mod (a, v, n) for i = 0, n - 1 do a[i] = a[i] % v end return a end

local V = ffi.new("int64_t", 3)
run("add", add, V)
run("subtract", sub, V)
run("multiply", mul, V)
run("and", band, V)
run("or", bor, V)
run("xor", bxor, V)
run("shl", shl, ffi.new("int64_t", 1))
run("shr", shr, ffi.new("int64_t", 1))
run("idiv", idiv, V)
run("mod", mod, V)
run("zero idiv", idiv, ffi.new("int64_t", 0))
run("zero mod", mod, ffi.new("int64_t", 0))

-- Small integer constants (immediate/K forms).
do
  local function init2 () local a = ffi.new("int64_t[?]", N); for i = 0, N - 1 do a[i] = i + 1 end return a end
  local function addc (a, n) for i = 0, n - 1 do a[i] = a[i] + 3 end return a end
  local function subc (a, n) for i = 0, n - 1 do a[i] = a[i] - 3 end return a end
  local function mulc (a, n) for i = 0, n - 1 do a[i] = a[i] * 3 end return a end
  local function bandc (a, n) for i = 0, n - 1 do a[i] = a[i] & 3 end return a end
  local function shlc (a, n) for i = 0, n - 1 do a[i] = a[i] << 2 end return a end
  local function idivc (a, n) for i = 0, n - 1 do a[i] = a[i] // 3 end return a end
  local function modc (a, n) for i = 0, n - 1 do a[i] = a[i] % 3 end return a end
  for name, f in pairs{ add = addc, sub = subc, mul = mulc,
                        band = bandc, shl = shlc, idiv = idivc,
                        mod = modc } do
    jit.off()
    local want = f(init2(), N)
    jit.flush()
    jit.opt.start("ir=0", "hotloop=1", "tint=1")
    jit.on()
    local a = init2()
    local before = jit.stats().tint_hits
    f(a, N)
    H.check(jit.stats().tint_hits > before, name .. " constant did not run")
    for i = 0, N - 1 do H.eq(a[i], want[i], name .. " constant " .. i) end
  end
end

-- Constant zero divisor uses the cdata INT64_MIN result.
do
  local function idivz (a, n) for i = 0, n - 1 do a[i] = a[i] // 0 end return a end
  local function modz (a, n) for i = 0, n - 1 do a[i] = a[i] % 0 end return a end
  for name, f in pairs{ idiv = idivz, mod = modz } do
    jit.off()
    local want = f(init(), N)
    jit.flush()
    jit.opt.start("ir=0", "hotloop=1", "tint=1")
    jit.on()
    local a = init()
    local before = jit.stats().tint_hits
    f(a, N)
    H.check(jit.stats().tint_hits > before, name .. " zero did not run")
    for i = 0, N - 1 do H.eq(a[i], want[i], name .. " zero " .. i) end
  end
end

-- uint64 destination with a cdata scalar.
do
  local U = ffi.new("uint64_t[?]", N)
  for i = 0, N - 1 do U[i] = i + 1 end
  jit.off()
  local want = ffi.new("uint64_t[?]", N)
  for i = 0, N - 1 do want[i] = U[i] * ffi.new("uint64_t", 3) end
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  mul(U, ffi.new("uint64_t", 3), N)
  H.check(jit.stats().tint_hits > before, "uint64 multiply did not run")
  for i = 0, N - 1 do H.eq(U[i], want[i], "uint64 " .. i) end
end

-- uint64 divide/modulo.
do
  local U = ffi.new("uint64_t[?]", N)
  for i = 0, N - 1 do U[i] = i + 1 end
  local V3 = ffi.new("uint64_t", 3)
  jit.off()
  local want = ffi.new("uint64_t[?]", N)
  for i = 0, N - 1 do want[i] = U[i] // V3 end
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  idiv(U, V3, N)
  H.check(jit.stats().tint_hits > before, "uint64 divide did not run")
  for i = 0, N - 1 do H.eq(U[i], want[i], "uint64 divide " .. i) end

  for i = 0, N - 1 do U[i] = i + 1 end
  jit.off()
  local wantm = ffi.new("uint64_t[?]", N)
  for i = 0, N - 1 do wantm[i] = U[i] % 3 end
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  before = jit.stats().tint_hits
  local function umod (a, _, n) for i = 0, n - 1 do a[i] = a[i] % 3 end return a end
  umod(U, nil, N)
  H.check(jit.stats().tint_hits > before, "uint64 modulo did not run")
  for i = 0, N - 1 do H.eq(U[i], wantm[i], "uint64 modulo " .. i) end
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/196_j2_i64_update ok")
