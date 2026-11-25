-- ffi/217_j2_narrow_field_upvalue_copy.lua -- J2 narrow-field copies through
-- an upvalue struct source or destination.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/217_j2_narrow_field_upvalue_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int32_t a; int16_t b; int8_t c; int64_t d; } NUC;
]]

-- Upvalue source, register destination.
do
  local U = ffi.new("NUC")
  U.a, U.b, U.c, U.d = 7, -3, 5, 4
  local D = ffi.new("NUC")
  D.a, D.b, D.c, D.d = 1, 2, 3, 4
  local function copy (d, n) for _ = 1, n do d.a = U.a end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "source upvalue copy did not run")
  H.eq(D.a, 7, "source upvalue int32")
  H.eq(D.d, 4, "source upvalue sibling")
end

-- Register source, upvalue destination.
do
  local U = ffi.new("NUC")
  U.a, U.b, U.c, U.d = 1, 2, 3, 4
  local D = ffi.new("NUC")
  D.a, D.b, D.c, D.d = 1, 9, 3, 4
  local function copy (d, n) for _ = 1, n do U.b = d.b end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "destination upvalue copy did not run")
  H.eq(U.b, 9, "destination upvalue int16")
  H.eq(U.d, 4, "destination upvalue sibling")
end

-- Both upvalues, cross-width int16->int8.
do
  local U = ffi.new("NUC")
  U.a, U.b, U.c, U.d = 1, 0x1234, 3, 4
  local function copy (n) for _ = 1, n do U.c = U.b end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(200)
  H.check(jit.stats().tint_hits > before, "both-upvalue copy did not run")
  H.eq(U.c, 0x34, "both upvalue int16->int8")
  H.eq(U.d, 4, "both upvalue sibling")
end

-- Cross-width int32->int8 truncation.
do
  local U = ffi.new("NUC")
  U.a, U.b, U.c, U.d = 100000, 2, 3, 4
  local D = ffi.new("NUC")
  D.a, D.b, D.c, D.d = 1, 2, 3, 4
  local function copy (d, n) for _ = 1, n do d.c = U.a end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "cross-width copy did not run")
  H.eq(D.c, -96, "cross-width int32->int8")
  H.eq(D.d, 4, "cross-width sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/217_j2_narrow_field_upvalue_copy ok")
