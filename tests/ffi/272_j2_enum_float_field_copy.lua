-- ffi/272_j2_enum_float_field_copy.lua -- J2 enum/float mixed field copies.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/272_j2_enum_float_field_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef enum { EFA = 1 } EF272;
typedef struct { EF272 e; float f; double d; int64_t z; } NF272;
]]

local function run (name, setup, f, check)
  local s = ffi.new("NF272"); s.e, s.f, s.d, s.z = 0, 0, 0, 4
  setup(s)
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  check(s)
  H.eq(s.z, 4, name .. ": sibling")
end

run("enum->float", function (s) s.e = 5 end, function (o, n)
  for _ = 1, n do o.f = o.e end
end, function (s) H.eq(s.f, 5.0, "enum->float") end)

run("float->enum", function (s) s.f = -3.9 end, function (o, n)
  for _ = 1, n do o.e = o.f end
end, function (s) H.eq(s.e, 4294967293, "float->enum") end)

run("enum->double", function (s) s.e = 7 end, function (o, n)
  for _ = 1, n do o.d = o.e end
end, function (s) H.eq(s.d, 7.0, "enum->double") end)

run("double->enum", function (s) s.d = 9.9 end, function (o, n)
  for _ = 1, n do o.e = o.d end
end, function (s) H.eq(s.e, 9, "double->enum") end)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/272_j2_enum_float_field_copy ok")
