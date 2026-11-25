-- ffi/198_j2_i64_field_update.lua -- J2 in-place updates of 8-byte integer
-- struct fields with constant scalars.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/198_j2_i64_field_update: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int64_t x; int64_t y; } FI;
typedef struct { uint64_t x; uint64_t y; } FU;
]]

local function run (name, f, ctype, init)
  local want = ffi.new(ctype)
  want.x, want.y = init, 77
  jit.off()
  f(want, 200)

  local s = ffi.new(ctype)
  s.x, s.y = init, 77
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(s.x, want.x, name .. ": field value")
  H.eq(s.y, 77, name .. ": sibling field preserved")
end

local function add (s, n) for _ = 1, n do s.x = s.x + 3 end end
local function sub (s, n) for _ = 1, n do s.x = s.x - 3 end end
local function mul (s, n) for _ = 1, n do s.x = s.x * 3 end end
local function band (s, n) for _ = 1, n do s.x = s.x & 3 end end
local function shl (s, n) for _ = 1, n do s.x = s.x << 2 end end

run("add", add, "FI", 10)
run("subtract", sub, "FI", 1000)
run("multiply", mul, "FI", 1)
run("and", band, "FI", 1)
run("shl", shl, "FI", 1)

local wu = ffi.new("FU")
wu.x, wu.y = 5, 77
jit.off()
mul(wu, 200)
local u = ffi.new("FU")
u.x, u.y = 5, 77
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
mul(u, 200)
H.check(jit.stats().tint_hits > before, "uint64 field did not run")
H.eq(u.x, wu.x, "uint64 field wrap")
H.eq(u.y, 77, "uint64 sibling preserved")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/198_j2_i64_field_update ok")
