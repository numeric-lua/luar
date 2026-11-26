-- ffi/238_j2_enum_field_fill.lua -- J2 enum field fills.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/238_j2_enum_field_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef enum { EA = 1, EB = 5 } EF;
typedef struct { EF e; int64_t d; } NEF;
]]

local function check (name, s, want)
  H.eq(s.e, want, name .. ": value")
  H.eq(s.d, 4, name .. ": sibling")
end

do
  local s = ffi.new("NEF"); s.e, s.d = 0, 4
  local function fill (o, n) for _ = 1, n do o.e = 5 end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, 200)
  H.check(jit.stats().tint_hits > before, "constant did not run")
  check("constant", s, 5)
end

do
  local s = ffi.new("NEF"); s.e, s.d = 0, 4
  local V = ffi.new("EF", 3)
  local function fill (o, v, n) for _ = 1, n do o.e = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, V, 200)
  H.check(jit.stats().tint_hits > before, "scalar did not run")
  check("scalar", s, 3)
end

do
  local U = ffi.new("NEF"); U.e, U.d = 0, 4
  local function fill (n) for _ = 1, n do U.e = 7 end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue did not run")
  check("upvalue", U, 7)
end

do
  local s = ffi.new("NEF"); s.e, s.d = 0, 4
  local function fill (o, n) for _ = 1, n do o.e = -1 end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, 200)
  H.check(jit.stats().tint_hits > before, "wrap did not run")
  check("wrap", s, 4294967295)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/238_j2_enum_field_fill ok")
