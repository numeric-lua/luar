-- ffi/239_j2_enum_field_copy.lua -- J2 enum field copies.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/239_j2_enum_field_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef enum { ECA = 1 } EC;
typedef struct { EC e; EC f; int32_t i; int64_t d; } NEC;
]]

local function check (name, s, e, f, i)
  H.eq(s.e, e, name .. ": e")
  H.eq(s.f, f, name .. ": f")
  H.eq(s.i, i, name .. ": i")
  H.eq(s.d, 4, name .. ": sibling")
end

do
  local s = ffi.new("NEC"); s.e, s.f, s.i, s.d = 0, 5, 7, 4
  local function copy (o, n) for _ = 1, n do o.e = o.f end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(s, 200)
  H.check(jit.stats().tint_hits > before, "enum->enum did not run")
  check("enum->enum", s, 5, 5, 7)
end

do
  local s = ffi.new("NEC"); s.e, s.f, s.i, s.d = 0, 5, 7, 4
  local function copy (o, n) for _ = 1, n do o.i = o.f end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(s, 200)
  H.check(jit.stats().tint_hits > before, "enum->int32 did not run")
  check("enum->int32", s, 0, 5, 5)
end

do
  local s = ffi.new("NEC"); s.e, s.f, s.i, s.d = 0, 5, 7, 4
  local function copy (o, n) for _ = 1, n do o.e = o.i end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(s, 200)
  H.check(jit.stats().tint_hits > before, "int32->enum did not run")
  check("int32->enum", s, 7, 5, 7)
end

do
  local U = ffi.new("NEC"); U.e, U.f, U.i, U.d = 0, 9, 7, 4
  local D = ffi.new("NEC"); D.e, D.f, D.i, D.d = 0, 5, 7, 4
  local function copy (o, n) for _ = 1, n do o.e = U.f end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "upvalue enum copy did not run")
  check("upvalue enum", D, 9, 5, 7)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/239_j2_enum_field_copy ok")
