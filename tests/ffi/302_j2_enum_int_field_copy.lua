-- ffi/302_j2_enum_int_field_copy.lua -- J2 enum/integer field copies.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/302_j2_enum_int_field_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef enum { EIC302 = 1 } EI302;
typedef struct { EI302 e; int64_t i; int32_t z; } NF302;
]]

local function run (name, setup, f, check)
  local s = ffi.new("NF302"); s.e, s.i, s.z = 0, 0, 4
  setup(s)
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  check(s)
  H.eq(s.z, 4, name .. ": sibling")
end

run("enum->int64", function (s) s.e = 7 end, function (o, n)
  for _ = 1, n do o.i = o.e end
end, function (s) H.eq(s.i, 7, "enum->int64") end)

run("int64->enum", function (s) s.i = -3 end, function (o, n)
  for _ = 1, n do o.e = o.i end
end, function (s) H.eq(s.e, 4294967293, "int64->enum") end)

do
  local U = ffi.new("NF302"); U.e, U.i, U.z = 5, 0, 4
  local D = ffi.new("NF302"); D.e, D.i, D.z = 0, 0, 4
  local function copy (o, n) for _ = 1, n do o.i = U.e end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "upvalue enum did not run")
  H.eq(D.i, 5, "upvalue enum")
  H.eq(D.z, 4, "upvalue enum sibling")
end

do
  local U = ffi.new("NF302"); U.e, U.i, U.z = 0, 9, 4
  local D = ffi.new("NF302"); D.e, D.i, D.z = 0, 0, 4
  local function copy (o, n) for _ = 1, n do o.e = U.i end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "upvalue int did not run")
  H.eq(D.e, 9, "upvalue int")
  H.eq(D.z, 4, "upvalue int sibling")
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/302_j2_enum_int_field_copy ok")
