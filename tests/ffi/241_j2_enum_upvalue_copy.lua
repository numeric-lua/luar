-- ffi/241_j2_enum_upvalue_copy.lua -- J2 enum field copies through upvalues.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/241_j2_enum_upvalue_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef enum { EUA = 1 } EUV;
typedef struct { EUV e; int32_t i; int64_t d; } NEUV;
]]

local function check (name, s, e, i)
  H.eq(s.e, e, name .. ": e")
  H.eq(s.i, i, name .. ": i")
  H.eq(s.d, 4, name .. ": sibling")
end

do
  local U = ffi.new("NEUV"); U.e, U.i, U.d = 9, 3, 4
  local D = ffi.new("NEUV"); D.e, D.i, D.d = 0, 0, 4
  local function copy (o, n) for _ = 1, n do o.e = U.e end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "upvalue enum->enum did not run")
  check("upvalue enum->enum", D, 9, 0)
end

do
  local U = ffi.new("NEUV"); U.e, U.i, U.d = 0, 7, 4
  local D = ffi.new("NEUV"); D.e, D.i, D.d = 0, 0, 4
  local function copy (o, n) for _ = 1, n do o.e = U.i end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "upvalue int->enum did not run")
  check("upvalue int->enum", D, 7, 0)
end

do
  local U = ffi.new("NEUV"); U.e, U.i, U.d = 0, 0, 4
  local D = ffi.new("NEUV"); D.e, D.i, D.d = 5, 7, 4
  local function copy (o, n) for _ = 1, n do U.i = o.e end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "upvalue enum->int did not run")
  check("upvalue enum->int", U, 0, 5)
  check("upvalue enum->int src", D, 5, 7)
end

do
  local U = ffi.new("NEUV"); U.e, U.i, U.d = 9, 0, 4
  local function copy (n) for _ = 1, n do U.e = U.e end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(200)
  H.check(jit.stats().tint_hits > before, "self enum copy did not run")
  check("self enum", U, 9, 0)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/241_j2_enum_upvalue_copy ok")
