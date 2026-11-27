-- ffi/248_j2_f64_field_copy.lua -- J2 double field copies.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/248_j2_f64_field_copy: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { double x; double y; int64_t d; } NF248;
]]

local function check (name, s, x, y)
  H.eq(s.x, x, name .. ": x")
  H.eq(s.y, y, name .. ": y")
  H.eq(s.d, 4, name .. ": sibling")
end

do
  local s = ffi.new("NF248"); s.x, s.y, s.d = 1, 2.5, 4
  local function copy (o, n) for _ = 1, n do o.x = o.y end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(s, 200)
  H.check(jit.stats().tint_hits > before, "x<-y did not run")
  check("x<-y", s, 2.5, 2.5)
end

do
  local s = ffi.new("NF248"); s.x, s.y, s.d = 1, 2.5, 4
  local function copy (o, n) for _ = 1, n do o.y = o.x end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(s, 200)
  H.check(jit.stats().tint_hits > before, "y<-x did not run")
  check("y<-x", s, 1, 1)
end

do
  local s = ffi.new("NF248"); s.x, s.y, s.d = 3.5, 2.5, 4
  local function copy (o, n) for _ = 1, n do o.x = o.x end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(s, 200)
  H.check(jit.stats().tint_hits > before, "self did not run")
  check("self", s, 3.5, 2.5)
end

do
  local S = ffi.new("NF248"); S.x, S.y, S.d = 1, 9.5, 4
  local D = ffi.new("NF248"); D.x, D.y, D.d = 0, 0, 4
  local function copy (a, b, n) for _ = 1, n do b.x = a.y end end
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  copy(S, D, 200)
  H.check(jit.stats().tint_hits > before, "cross did not run")
  check("cross", D, 9.5, 0)
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/248_j2_f64_field_copy ok")
