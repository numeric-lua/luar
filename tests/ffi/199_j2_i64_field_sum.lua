-- ffi/199_j2_i64_field_sum.lua -- J2 reductions over 8-byte struct fields
-- into a cdata accumulator.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/199_j2_i64_field_sum: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int64_t x; int64_t y; } FS;
typedef struct { uint64_t x; uint64_t y; } FU;
]]

local function run (name, f, init)
  local w = ffi.new("FS")
  w.x, w.y = 7, 9
  jit.off()
  local want = f(w, 200)

  local s = ffi.new("FS")
  s.x, s.y = 7, 9
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local got = f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(got, want, name .. ": accumulator")
  H.eq(type(got), "cdata", name .. ": cdata accumulator")
  H.eq(s.x, 7, name .. ": field unchanged")
  H.eq(s.y, 9, name .. ": sibling unchanged")
end

local function add (o, n)
  local a = ffi.new("int64_t", 0)
  for _ = 1, n do a = a + o.x end
  return a
end
local function sub (o, n)
  local a = ffi.new("int64_t", 100000)
  for _ = 1, n do a = a - o.x end
  return a
end
local function mul (o, n)
  local a = ffi.new("int64_t", 1)
  for _ = 1, n do a = a * o.x end
  return a
end
local function band (o, n)
  local a = ffi.new("int64_t", -1)
  for _ = 1, n do a = a & o.x end
  return a
end
local function shl (o, n)
  local a = ffi.new("int64_t", 1)
  for _ = 1, n do a = a << o.x end
  return a
end

run("add", add)
run("subtract", sub)
run("multiply", mul)
run("and", band)
run("shift", shl)

-- Upvalue struct field.
do
  local U
  local function add_u (n) local a = ffi.new("int64_t", 0); for _ = 1, n do a = a + U.x end return a end
  local function sub_u (n) local a = ffi.new("int64_t", 100000); for _ = 1, n do a = a - U.x end return a end
  local function mul_u (n) local a = ffi.new("int64_t", 1); for _ = 1, n do a = a * U.x end return a end
  local function band_u (n) local a = ffi.new("int64_t", -1); for _ = 1, n do a = a & U.x end return a end
  local function shl_u (n) local a = ffi.new("int64_t", 1); for _ = 1, n do a = a << U.x end return a end
  for name, f in pairs{ add = add_u, sub = sub_u, mul = mul_u,
                        band = band_u, shl = shl_u } do
    U = ffi.new("FS")
    U.x, U.y = 7, 9
    jit.off()
    local want = f(200)
    U = ffi.new("FS")
    U.x, U.y = 7, 9
    jit.flush()
    jit.opt.start("ir=0", "hotloop=1", "tint=1")
    jit.on()
    local before = jit.stats().tint_hits
    local got = f(200)
    H.check(jit.stats().tint_hits > before, name .. " upvalue field sum did not run")
    H.eq(got, want, name .. " upvalue field sum")
    H.eq(U.x, 7, name .. " upvalue field unchanged")
    H.eq(U.y, 9, name .. " upvalue sibling unchanged")
  end
end

-- Divide/modulo reductions over fields.
do
  local function check (name, f, ctype, xval)
    local w = ffi.new(ctype)
    w.x, w.y = xval, 9
    jit.off()
    local want = f(w, 200)
    local s = ffi.new(ctype)
    s.x, s.y = xval, 9
    jit.flush()
    jit.opt.start("ir=0", "hotloop=1", "tint=1")
    jit.on()
    local before = jit.stats().tint_hits
    local got = f(s, 200)
    H.check(jit.stats().tint_hits > before, name .. " did not run")
    H.eq(got, want, name .. " value")
  end
  local function idiv (o, n) local a = ffi.new("int64_t", 1000000); for _ = 1, n do a = a // o.x end return a end
  local function mod (o, n) local a = ffi.new("int64_t", 1000000); for _ = 1, n do a = a % o.x end return a end
  local function uidiv (o, n) local a = ffi.new("uint64_t", 1000000); for _ = 1, n do a = a // o.x end return a end
  local function umod (o, n) local a = ffi.new("uint64_t", 1000000); for _ = 1, n do a = a % o.x end return a end
  check("field idiv", idiv, "FS", 3)
  check("field mod", mod, "FS", 3)
  check("field uint idiv", uidiv, "FU", 3)
  check("field uint mod", umod, "FU", 3)
  check("field idiv zero", idiv, "FS", 0)
  check("field mod zero", mod, "FS", 0)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/199_j2_i64_field_sum ok")
