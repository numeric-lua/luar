-- ffi/233_j2_bool_field_copy.lua -- J2 _Bool field copies.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/233_j2_bool_field_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { _Bool b; _Bool c; int64_t d; } NBC;
]]

local function check (name, s, b, c, d)
  H.eq(s.b, b, name .. ": b")
  H.eq(s.c, c, name .. ": c")
  H.eq(s.d, d, name .. ": sibling")
end

do
  local s = ffi.new("NBC"); s.b, s.c, s.d = true, false, 4
  local function copy (o, n) for _ = 1, n do o.b = o.c end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(s, 200)
  H.check(jit.stats().tint_hits > before, "true->false did not run")
  check("true->false", s, false, false, 4)
end

do
  local s = ffi.new("NBC"); s.b, s.c, s.d = false, true, 4
  local function copy (o, n) for _ = 1, n do o.b = o.c end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(s, 200)
  H.check(jit.stats().tint_hits > before, "false->true did not run")
  check("false->true", s, true, true, 4)
end

do
  local U = ffi.new("NBC"); U.b, U.c, U.d = false, true, 4
  local D = ffi.new("NBC"); D.b, D.c, D.d = false, false, 4
  local function copy (o, n) for _ = 1, n do o.b = U.c end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "upvalue source did not run")
  check("upvalue source", D, true, false, 4)
end

do
  local U = ffi.new("NBC"); U.b, U.c, U.d = false, true, 4
  local D = ffi.new("NBC"); D.b, D.c, D.d = true, false, 4
  local function copy (o, n) for _ = 1, n do U.b = o.c end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(D, 200)
  H.check(jit.stats().tint_hits > before, "upvalue destination did not run")
  check("upvalue destination", U, false, true, 4)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/233_j2_bool_field_copy ok")
