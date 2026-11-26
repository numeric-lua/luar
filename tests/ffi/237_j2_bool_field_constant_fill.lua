-- ffi/237_j2_bool_field_constant_fill.lua -- J2 _Bool field fills from
-- constant booleans.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/237_j2_bool_field_constant_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { _Bool b; int64_t d; } NBK;
]]

local function check (name, s, want)
  H.eq(s.b, want, name .. ": value")
  H.eq(s.d, 4, name .. ": sibling")
end

do
  local s = ffi.new("NBK"); s.b, s.d = false, 4
  local function fill (o, n) for _ = 1, n do o.b = true end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, 200)
  H.check(jit.stats().tint_hits > before, "true constant did not run")
  check("true constant", s, true)
end

do
  local s = ffi.new("NBK"); s.b, s.d = true, 4
  local function fill (o, n) for _ = 1, n do o.b = false end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, 200)
  H.check(jit.stats().tint_hits > before, "false constant did not run")
  check("false constant", s, false)
end

do
  local U = ffi.new("NBK"); U.b, U.d = false, 4
  local function fill (n) for _ = 1, n do U.b = true end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue true did not run")
  check("upvalue true", U, true)
end

do
  local U = ffi.new("NBK"); U.b, U.d = true, 4
  local function fill (n) for _ = 1, n do U.b = false end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(200)
  H.check(jit.stats().tint_hits > before, "upvalue false did not run")
  check("upvalue false", U, false)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/237_j2_bool_field_constant_fill ok")
