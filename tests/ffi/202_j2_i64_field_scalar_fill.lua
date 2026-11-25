-- ffi/202_j2_i64_field_scalar_fill.lua -- J2 8-byte field fills from cdata.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/202_j2_i64_field_scalar_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int64_t x; int64_t y; } SFS;
typedef struct { uint64_t x; uint64_t y; } SFU;
]]

-- Register struct + register cdata scalar.
do
  local s = ffi.new("SFS")
  s.x, s.y = 1, 9
  local V = ffi.new("int64_t", 7)
  local function fill (o, v, n) for _ = 1, n do o.x = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "register scalar fill did not run")
  H.eq(s.x, 7, "register scalar value")
  H.eq(s.y, 9, "register scalar sibling")
end

-- Negative cdata scalar.
do
  local s = ffi.new("SFS")
  s.x, s.y = 1, 9
  local V = ffi.new("int64_t", -7)
  local function fill (o, v, n) for _ = 1, n do o.x = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "negative scalar fill did not run")
  H.eq(s.x, -7, "negative scalar value")
  H.eq(s.y, 9, "negative scalar sibling")
end

-- Upvalue struct + upvalue cdata scalar.
do
  local U = ffi.new("SFS")
  U.x, U.y = 1, 9
  local V = ffi.new("int64_t", 7)
  local function fill (n) for _ = 1, n do U.x = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue scalar fill did not run")
  H.eq(U.x, 7, "upvalue scalar value")
  H.eq(U.y, 9, "upvalue scalar sibling")
end

-- uint64 field.
do
  local U = ffi.new("SFU")
  U.x, U.y = 1, 9
  local V = ffi.new("uint64_t", 7)
  local function fill (o, v, n) for _ = 1, n do o.x = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(U, V, 200)
  H.check(jit.stats().tint_hits > before, "uint64 scalar fill did not run")
  H.eq(U.x, 7, "uint64 scalar value")
  H.eq(U.y, 9, "uint64 scalar sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/202_j2_i64_field_scalar_fill ok")
