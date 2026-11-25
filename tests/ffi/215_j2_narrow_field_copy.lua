-- ffi/215_j2_narrow_field_copy.lua -- J2 narrow-field copies.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/215_j2_narrow_field_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct {
  int32_t a; int32_t aa;
  int16_t b; int16_t bb;
  int8_t c; int8_t cc;
  int64_t d;
} NFC;
]]

-- Direct assertions.
do
  local s = ffi.new("NFC")
  s.a, s.aa = 1, 7
  s.b, s.bb = 2, -3
  s.c, s.cc = 3, 5
  s.d = 4
  local function copy (o, n) for _ = 1, n do o.a = o.aa end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(s, 200)
  H.check(jit.stats().tint_hits > before, "int32 copy did not run")
  H.eq(s.a, 7, "int32 copy")
  H.eq(s.d, 4, "int32 sibling")
end

do
  local s = ffi.new("NFC")
  s.b, s.bb = 2, -3
  s.d = 4
  local function copy (o, n) for _ = 1, n do o.b = o.bb end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(s, 200)
  H.check(jit.stats().tint_hits > before, "int16 copy did not run")
  H.eq(s.b, -3, "int16 copy")
  H.eq(s.d, 4, "int16 sibling")
end

do
  local s = ffi.new("NFC")
  s.c, s.cc = 3, 5
  s.d = 4
  local function copy (o, n) for _ = 1, n do o.c = o.cc end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(s, 200)
  H.check(jit.stats().tint_hits > before, "int8 copy did not run")
  H.eq(s.c, 5, "int8 copy")
  H.eq(s.d, 4, "int8 sibling")
end

do
  local s = ffi.new("NFC")
  s.a = 9
  s.d = 4
  local function copy (o, n) for _ = 1, n do o.a = o.a end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(s, 200)
  H.check(jit.stats().tint_hits > before, "self copy did not run")
  H.eq(s.a, 9, "self copy")
  H.eq(s.d, 4, "self sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/215_j2_narrow_field_copy ok")
