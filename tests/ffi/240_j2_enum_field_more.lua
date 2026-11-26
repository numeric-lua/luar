-- ffi/240_j2_enum_field_more.lua -- J2 enum field fill/copy edge cases.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/240_j2_enum_field_more: skipped (no native target)")
  return
end

ffi.cdef[[
typedef enum { EMA = 1 } EM;
typedef struct { EM e; EM f; int64_t d; } NEM;
]]

local function check (name, s, e, f)
  H.eq(s.e, e, name .. ": e")
  H.eq(s.f, f, name .. ": f")
  H.eq(s.d, 4, name .. ": sibling")
end

do
  local s = ffi.new("NEM"); s.e, s.f, s.d = 1, 2, 4
  local function fill (o, n) for _ = 1, n do o.e = 0 end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, 200)
  H.check(jit.stats().tint_hits > before, "zero fill did not run")
  check("zero", s, 0, 2)
end

do
  local s = ffi.new("NEM"); s.e, s.f, s.d = 1, 2, 4
  local V = ffi.new("EM", 0xffffffff)
  local function fill (o, v, n) for _ = 1, n do o.e = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "max fill did not run")
  check("max", s, 0xffffffff, 2)
end

do
  local U = ffi.new("NEM"); U.e, U.f, U.d = 1, 2, 4
  local function fill (n) for _ = 1, n do U.e = 7 end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue fill did not run")
  check("upvalue", U, 7, 2)
end

do
  local s = ffi.new("NEM"); s.e, s.f, s.d = 1, 9, 4
  local function copy (o, n) for _ = 1, n do o.e = o.f end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(s, 200)
  H.check(jit.stats().tint_hits > before, "enum copy did not run")
  check("copy", s, 9, 9)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/240_j2_enum_field_more ok")
