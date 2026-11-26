-- ffi/232_j2_bool_field_boolean_fill.lua -- J2 _Bool field fills from
-- Lua booleans.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/232_j2_bool_field_boolean_fill: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { _Bool b; int64_t d; } NBB;
]]

local function check (name, s, want)
  H.eq(s.b, want, name .. ": value")
  H.eq(s.d, 4, name .. ": sibling")
end

do
  local s = ffi.new("NBB"); s.b, s.d = false, 4
  local function fill (o, v, n) for _ = 1, n do o.b = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, true, 200)
  H.check(jit.stats().tint_hits > before, "true register did not run")
  check("true register", s, true)
end

do
  local s = ffi.new("NBB"); s.b, s.d = true, 4
  local function fill (o, v, n) for _ = 1, n do o.b = v end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, false, 200)
  H.check(jit.stats().tint_hits > before, "false register did not run")
  check("false register", s, false)
end

do
  local s = ffi.new("NBB"); s.b, s.d = false, 4
  local V = true
  local function fill (o, n) for _ = 1, n do o.b = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, 200)
  H.check(jit.stats().tint_hits > before, "true upvalue did not run")
  check("true upvalue", s, true)
end

do
  local s = ffi.new("NBB"); s.b, s.d = true, 4
  local V = false
  local function fill (o, n) for _ = 1, n do o.b = V end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  fill(s, 200)
  H.check(jit.stats().tint_hits > before, "false upvalue did not run")
  check("false upvalue", s, false)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/232_j2_bool_field_boolean_fill ok")
