-- ffi/263_j2_f64_field_update_edges.lua -- J2 double field update edge cases.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/263_j2_f64_field_update_edges: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { double x; int64_t d; } NF263;
]]

local function run (name, init, f, check)
  local w = ffi.new("NF263"); w.x, w.d = init, 4
  jit.off(); f(w, 200)
  local want = w.x
  local s = ffi.new("NF263"); s.x, s.d = init, 4
  jit.flush(); jit.opt.start("ir=0", "hotloop=1", "tint=1"); jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  check(s, want)
  H.eq(s.d, 4, name .. ": sibling")
end

run("large mul", 1e300, function (o, n)
  for _ = 1, n do o.x = o.x * 1e10 end
end, function (s, want) H.eq(s.x, want, "large") end)

run("small div", 1e-300, function (o, n)
  for _ = 1, n do o.x = o.x / 1e10 end
end, function (s, want) H.eq(s.x, want, "small") end)

run("fraction add", 0.25, function (o, n)
  for _ = 1, n do o.x = o.x + 0.25 end
end, function (s, want) H.eq(s.x, want, "fraction") end)

run("negative zero", -0.0, function (o, n)
  for _ = 1, n do o.x = o.x + 0.0 end
end, function (s, want) H.eq(s.x, want, "zero") end)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/263_j2_f64_field_update_edges ok")
