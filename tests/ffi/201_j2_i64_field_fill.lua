-- ffi/201_j2_i64_field_fill.lua -- J2 8-byte struct-field constant fills.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/201_j2_i64_field_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int64_t x; int64_t y; } FFS;
typedef struct { uint64_t x; uint64_t y; } FFU;
]]

local function run (name, f, ctype, value)
  local s = ffi.new(ctype)
  s.x, s.y = 1, 9
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(s.x, value, name .. ": field value")
  H.eq(s.y, 9, name .. ": sibling preserved")
end

local function fill (s, n) for _ = 1, n do s.x = 7 end end
run("register constant", fill, "FFS", 7)

local function fill_neg (s, n) for _ = 1, n do s.x = -7 end end
run("negative constant", fill_neg, "FFS", -7)

-- Upvalue struct destination.
do
  local U = ffi.new("FFS")
  U.x, U.y = 1, 9
  local function up (n) for _ = 1, n do U.x = 7 end end
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  up(200)
  H.check(jit.stats().tint_hits > before, "upvalue constant fill did not run")
  H.eq(U.x, 7, "upvalue field")
  H.eq(U.y, 9, "upvalue sibling")
end

-- uint64 field.
do
  local U = ffi.new("FFU")
  U.x, U.y = 1, 9
  local function up (n) for _ = 1, n do U.x = 7 end end
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  up(200)
  H.check(jit.stats().tint_hits > before, "uint64 constant fill did not run")
  H.eq(U.x, 7, "uint64 field")
  H.eq(U.y, 9, "uint64 sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/201_j2_i64_field_fill ok")
