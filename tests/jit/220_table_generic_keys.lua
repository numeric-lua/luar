-- jit/220_table_generic_keys.lua -- generic table copies/fills at hash and
-- negative integer keys, plus the sparse nil-clear key fix.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/220_table_generic_keys: skipped (no native target)")
  return
end

local N = 200
local A = {}
for i = -N, -1 do A[i] = "v" .. i end

local D = {}
local function copy_neg (n) for i = -n, -1 do D[i] = A[i] end end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
copy_neg(N)
H.check(jit.stats().tint_hits > before, "negative copy did not run")
for i = -N, -1 do H.eq(D[i], A[i], "copy " .. i) end

local function fill_neg (n) for i = -n, -1 do D[i] = "x" end end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
fill_neg(N)
H.check(jit.stats().tint_hits > before, "negative fill did not run")
for i = -N, -1 do H.eq(D[i], "x", "fill " .. i) end

local function clear_neg (n) for i = -n, -1 do D[i] = nil end end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
clear_neg(N)
H.check(jit.stats().tint_hits > before, "negative nil clear did not run")
for i = -N, -1 do H.eq(D[i], nil, "clear " .. i) end

-- Sparse hash key ranges use the Lua key, not the zero-based loop offset.
local t = {}
for i = 1000, 1200 do t[i] = i end
t[999], t[1201] = "low", "high"
local function clear_high (a, lo, hi) for i = lo, hi do a[i] = nil end end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
clear_high(t, 1000, 1200)
H.check(jit.stats().tint_hits > before, "sparse positive clear did not run")
H.eq(t[999], "low", "below range")
H.eq(t[1000], nil, "range start")
H.eq(t[1200], nil, "range end")
H.eq(t[1201], "high", "above range")

-- Numeric constant fill through an upvalue destination.
local U = {}
local function fill_up_value (n) for i = 1, n do U[i] = 7 end end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
fill_up_value(N)
H.check(jit.stats().tint_hits > before, "upvalue numeric fill did not run")
for i = 1, N do H.eq(U[i], 7, "upvalue numeric " .. i) end

-- Identity fill through an upvalue destination.
local V = {}
local function fill_up_index (n) for i = 1, n do V[i] = i end end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
fill_up_index(N)
H.check(jit.stats().tint_hits > before, "upvalue identity fill did not run")
for i = 1, N do H.eq(V[i], i, "upvalue identity " .. i) end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/220_table_generic_keys ok")
