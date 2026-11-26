-- ffi/226_j2_narrow_field_divmod_update.lua -- J2 narrow-field divide/modulo.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/226_j2_narrow_field_divmod_update: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int32_t a; int16_t b; int8_t c; uint32_t ua; } NFD;
]]

local function init (s, v)
  s.a, s.b, s.c, s.ua = v, v, v, v
end

local function run (name, f, get)
  local w = ffi.new("NFD"); init(w, 1000000)
  jit.off(); f(w, 200)
  local want = get(w)

  local s = ffi.new("NFD"); init(s, 1000000)
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(get(s), want, name .. ": value")
end

run("int32 div", function (o, n) for _ = 1, n do o.a = o.a // 3 end end,
    function (s) return s.a end)
run("int32 mod", function (o, n) for _ = 1, n do o.a = o.a % 3 end end,
    function (s) return s.a end)
run("int16 div", function (o, n) for _ = 1, n do o.b = o.b // 3 end end,
    function (s) return s.b end)
run("int8 div", function (o, n) for _ = 1, n do o.c = o.c // 3 end end,
    function (s) return s.c end)
run("uint32 div", function (o, n) for _ = 1, n do o.ua = o.ua // 3 end end,
    function (s) return s.ua end)

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/226_j2_narrow_field_divmod_update ok")
