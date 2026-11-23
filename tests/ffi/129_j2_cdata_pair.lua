-- ffi/129_j2_cdata_pair.lua -- default J2 two-source cdata reductions.
--
-- `s = s + a[i] + b[i]`, the subtract form and the dot/square forms over
-- bounded cdata arrays now run through a C helper in the default tier.  The
-- helper must preserve the interpreter's per-operation int wrap/float
-- promotion, the post-loop element temporaries and the loop-control slots,
-- and must fall back for pointers, mixed table/cdata sources and bounds
-- failures.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/129_j2_cdata_pair: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/129_j2_cdata_pair: skipped (no native target)")
  return
end

local function bits (x) return string.pack("d", x) end
local function same (a, b)
  if math.type(a) ~= math.type(b) then return false end
  if math.type(a) == "float" then
    if a ~= a and b ~= b then return true end
    if a ~= a or b ~= b then return false end
    return bits(a) == bits(b)
  end
  return a == b
end

local N = 257
local I = ffi.new("int32_t[?]", N)
local U = ffi.new("uint32_t[?]", N)
local S8 = ffi.new("int8_t[?]", N)
local D = ffi.new("double[?]", N)
local F = ffi.new("float[?]", N)
for i = 0, N - 1 do
  I[i] = i * 37 - 5000
  U[i] = 0x80000000 + i * 7
  S8[i] = i * 5 - 600
  D[i] = i * 0.25 - 3.5
  F[i] = i * 0.5 - 1.25
end

local function pair_add (a, b, n)
  local s = 0
  for i = 0, n - 1 do s = s + a[i] + b[i] end
  return s
end

local function pair_add_f (a, b, n)
  local s = 0.0
  for i = 0, n - 1 do s = s + a[i] + b[i] end
  return s
end

local function pair_sub (a, b, n)
  local s = 0
  for i = 0, n - 1 do s = s + a[i] - b[i] end
  return s
end

local function dot (a, b, n)
  local s = 0
  for i = 0, n - 1 do s = s + a[i] * b[i] end
  return s
end

local function dot_f (a, b, n)
  local s = 0.0
  for i = 0, n - 1 do s = s + a[i] * b[i] end
  return s
end

local function square (a, n)
  local s = 0.0
  for i = 0, n - 1 do s = s + a[i] * a[i] end
  return s
end

local function pair_obs (a, b, n)
  local s, t = 0, 0
  for i = 0, n - 1 do
    t = s + a[i]
    s = t + b[i]
  end
  return s, t
end

local function dot_obs (a, b, n)
  local s, t = 0, 0
  for i = 0, n - 1 do
    t = a[i] * b[i]
    s = s + t
  end
  return s, t
end

local function pair_step (a, b, n)
  local s = 0
  for i = 0, n - 1, 3 do s = s + a[i] + b[i] end
  return s
end

local function pair_down (a, b, n)
  local s = 0
  for i = n - 1, 0, -2 do s = s + a[i] + b[i] end
  return s
end

local function dot_down (a, b, n)
  local s = 0
  for i = n - 1, 0, -3 do s = s + a[i] * b[i] end
  return s
end

local function run (name, f, ...)
  local args = { ... }
  jit.flush()
  jit.off()
  local want = { pcall(f, table.unpack(args)) }
  jit.flush()
  jit.on()
  local before = jit.stats().tint_hits
  local got = { pcall(f, table.unpack(args)) }
  got = { pcall(f, table.unpack(args)) }  -- the second call hits the helper
  local hits = jit.stats().tint_hits - before
  H.eq(got[1], want[1], name .. ": error status")
  H.eq(#got, #want, name .. ": result count")
  if got[1] then
    for i = 2, #want do
      H.check(same(got[i], want[i]),
              name .. ": result " .. i .. " differs: " ..
              tostring(got[i]) .. " ~= " .. tostring(want[i]))
    end
  else
    H.check(tostring(got[2]):find("out of bounds", 1, true) ~= nil or
            tostring(got[2]) == tostring(want[2]),
            name .. ": error message differs")
  end
  return hits
end

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()

H.check(run("int add", pair_add, I, I, N) > 0, "int pair add did not use tint")
H.check(run("uint add", pair_add, U, U, N) > 0, "uint pair add did not use tint")
H.check(run("int8 add", pair_add, S8, S8, N) > 0, "int8 pair add did not use tint")
H.check(run("double add", pair_add, D, D, N) > 0, "double pair add did not use tint")
H.check(run("float add", pair_add, F, F, N) > 0, "float pair add did not use tint")
H.check(run("mixed add", pair_add, I, D, N) > 0, "mixed pair add did not use tint")
H.check(run("float acc", pair_add_f, I, I, N) > 0,
        "float accumulator pair add did not use tint")
H.check(run("int sub", pair_sub, I, I, N) > 0, "pair subtract did not use tint")
H.check(run("int dot", dot, I, I, N) > 0, "int dot product did not use tint")
H.check(run("double dot", dot, D, D, N) > 0, "double dot product did not use tint")
H.check(run("mixed dot", dot, I, D, N) > 0, "mixed dot product did not use tint")
H.check(run("dot float acc", dot_f, I, I, N) > 0,
        "float accumulator dot product did not use tint")
H.check(run("square", square, D, N) > 0, "square sum did not use tint")
-- The generic pair recognizer needs distinct element registers; a shape
-- that reuses one stays correct on the interpreter.
run("pair temps fallback", pair_obs, I, I, N)
H.check(run("dot temps", dot_obs, I, I, N) > 0,
        "observable dot temporaries did not use tint")
H.check(run("pair stride", pair_step, I, I, N) > 0,
        "strided pair sum did not use tint")
H.check(run("pair down", pair_down, I, I, N) > 0,
        "descending pair sum did not use tint")
H.check(run("dot down", dot_down, I, I, N) > 0,
        "descending dot product did not use tint")

-- Upvalue sources are re-resolved by the helper on every entry.
do
  local ua, ub = I, D
  local function up_pair (n)
    local s = 0.0
    for i = 0, n - 1 do s = s + ua[i] + ub[i] end
    return s
  end
  H.check(run("upvalue pair", up_pair, N) > 0,
          "upvalue pair sum did not use tint")
end

-- Pointers have no length, so both sources must stay interpreted (or the
-- helper must decline) while producing the same value.
do
  local pa = ffi.cast("int32_t *", I)
  local pb = ffi.cast("int32_t *", I)
  local function ptr_pair (a, b, n)
    local s = 0
    for i = 0, n - 1 do s = s + a[i] + b[i] end
    return s
  end
  jit.flush()
  jit.off()
  local want = ptr_pair(pa, pb, N)
  jit.flush()
  jit.on()
  H.eq(ptr_pair(pa, pb, N), want, "pointer pair fallback")
end

-- A table/cdata mixed pair is not a common reduction and stays interpreted.
do
  local function mixed (a, b, n)
    local s = 0
    for i = 0, n - 1 do s = s + a[i] + b[i] end
    return s
  end
  local t = {}
  for i = 0, N - 1 do t[i] = I[i] end
  jit.flush()
  jit.off()
  local want = mixed(I, t, N)
  jit.flush()
  jit.on()
  H.eq(mixed(I, t, N), want, "table/cdata mixed pair fallback")
end

-- Out-of-bounds runs must raise exactly like the interpreter.
H.check(run("oob pair", pair_add, I, I, N + 1) >= 0, "oob pair status")
H.check(run("oob dot", dot, I, I, N + 1) >= 0, "oob dot status")

jit.flush()
jit.opt.start("ir=0", "hotloop=56")
jit.on()
print("ffi/129_j2_cdata_pair ok")
