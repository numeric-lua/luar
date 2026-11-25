-- ffi/200_j2_i64_field_copy.lua -- J2 8-byte struct-field copies.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/200_j2_i64_field_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int64_t x; int64_t y; } CS;
typedef struct { uint64_t x; uint64_t y; } CU;
]]

local function run (name, f, ctype, x, y, wantx, wanty)
  local s = ffi.new(ctype)
  s.x, s.y = x, y
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(s.x, wantx, name .. ": destination field")
  H.eq(s.y, wanty, name .. ": sibling field")
end

local function copy_same (s, n) for _ = 1, n do s.x = s.y end end
local function self_copy (s, n) for _ = 1, n do s.x = s.x end end

run("same struct", copy_same, "CS", 1, 42, 42, 42)
run("self copy", self_copy, "CS", 7, 9, 7, 9)
run("uint64", copy_same, "CU", 1, 42, 42, 42)

-- Different struct types.
do
  local S = ffi.new("CS")
  local D = ffi.new("CU")
  S.x, S.y = 1, 42
  D.x, D.y = 0, 77
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  local function cross (a, b, n) for _ = 1, n do b.x = a.y end end
  cross(S, D, 200)
  H.check(jit.stats().tint_hits > before, "cross-struct copy did not run")
  H.eq(D.x, 42, "cross destination")
  H.eq(D.y, 77, "cross sibling")
  H.eq(S.x, 1, "cross source")
end

-- Missing field types fall back and raise normally.
do
  local b = ffi.new("CS")
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local ok = pcall(function ()
    local function bad (s, n) for _ = 1, n do s.no = s.x end end
    bad(b, 200)
  end)
  H.check(not ok, "missing field must raise")
end

-- Upvalue struct sources/destinations.
do
  local U = ffi.new("CS")
  U.x, U.y = 11, 22
  local D = ffi.new("CS")
  D.x, D.y = 0, 77
  local function from_u (d, n) for _ = 1, n do d.x = U.y end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  from_u(D, 200)
  H.check(jit.stats().tint_hits > before, "source upvalue copy did not run")
  H.eq(D.x, 22, "source upvalue destination")
  H.eq(D.y, 77, "source upvalue sibling")

  local function to_u (n) for _ = 1, n do U.x = D.y end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  before = jit.stats().tint_hits
  to_u(200)
  H.check(jit.stats().tint_hits > before, "destination upvalue copy did not run")
  H.eq(U.x, 77, "destination upvalue")
  H.eq(U.y, 22, "destination upvalue sibling")

  local V = ffi.new("CS")
  V.x, V.y = 1, 55
  local function both (v, n) for _ = 1, n do U.x = v.y end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  before = jit.stats().tint_hits
  both(V, 200)
  H.check(jit.stats().tint_hits > before, "two-upvalue copy did not run")
  H.eq(U.x, 55, "two-upvalue destination")
  H.eq(V.y, 55, "two-upvalue source")

  local function same (n) for _ = 1, n do U.x = U.y end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  before = jit.stats().tint_hits
  same(200)
  H.check(jit.stats().tint_hits > before, "same-upvalue copy did not run")
  H.eq(U.x, 22, "same-upvalue field")
  H.eq(U.y, 22, "same-upvalue sibling")
end

-- uint64 upvalue copy.
do
  local U = ffi.new("CU")
  U.x, U.y = 3, 44
  local D = ffi.new("CU")
  D.x, D.y = 0, 99
  local function copy (n) for _ = 1, n do D.x = U.y end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(200)
  H.check(jit.stats().tint_hits > before, "uint64 upvalue copy did not run")
  H.eq(D.x, 44, "uint64 upvalue destination")
  H.eq(D.y, 99, "uint64 upvalue sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/200_j2_i64_field_copy ok")
