-- ffi/198_j2_i64_field_update.lua -- J2 in-place updates of 8-byte integer
-- struct fields with constant scalars.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

if jit.status() ~= true then
  print("ffi/198_j2_i64_field_update: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct { int64_t x; int64_t y; } FI;
typedef struct { uint64_t x; uint64_t y; } FU;
]]

local function run (name, f, ctype, init)
  local want = ffi.new(ctype)
  want.x, want.y = init, 77
  jit.off()
  f(want, 200)

  local s = ffi.new(ctype)
  s.x, s.y = init, 77
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "tint=1")
  jit.on()
  local before = jit.stats().tint_hits
  f(s, 200)
  H.check(jit.stats().tint_hits > before, name .. ": intrinsic did not run")
  H.eq(s.x, want.x, name .. ": field value")
  H.eq(s.y, 77, name .. ": sibling field preserved")
end

local function add (s, n) for _ = 1, n do s.x = s.x + 3 end end
local function sub (s, n) for _ = 1, n do s.x = s.x - 3 end end
local function mul (s, n) for _ = 1, n do s.x = s.x * 3 end end
local function band (s, n) for _ = 1, n do s.x = s.x & 3 end end
local function shl (s, n) for _ = 1, n do s.x = s.x << 2 end end
local function idiv (s, n) for _ = 1, n do s.x = s.x // 3 end end
local function mod (s, n) for _ = 1, n do s.x = s.x % 3 end end
local function idivz (s, n) for _ = 1, n do s.x = s.x // 0 end end
local function modz (s, n) for _ = 1, n do s.x = s.x % 0 end end

run("add", add, "FI", 10)
run("subtract", sub, "FI", 1000)
run("multiply", mul, "FI", 1)
run("and", band, "FI", 1)
run("shl", shl, "FI", 1)
run("idiv", idiv, "FI", 1000000)
run("mod", mod, "FI", 1000000)
run("idiv zero", idivz, "FI", 1000000)
run("mod zero", modz, "FI", 1000000)

local wu = ffi.new("FU")
wu.x, wu.y = 5, 77
jit.off()
mul(wu, 200)
local u = ffi.new("FU")
u.x, u.y = 5, 77
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
mul(u, 200)
H.check(jit.stats().tint_hits > before, "uint64 field did not run")
H.eq(u.x, wu.x, "uint64 field wrap")
H.eq(u.y, 77, "uint64 sibling preserved")

local ud = ffi.new("FU")
ud.x, ud.y = 1000000, 77
jit.off()
idiv(ud, 200)
local udw = ffi.new("FU")
udw.x, udw.y = 1000000, 77
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
idiv(udw, 200)
H.check(jit.stats().tint_hits > before, "uint64 field divide did not run")
H.eq(udw.x, ud.x, "uint64 field divide")
H.eq(udw.y, 77, "uint64 field divide sibling")

-- Field updates with a cdata scalar register/upvalue.
do
  local V = ffi.new("int64_t", 3)
  local function add_v (s, n) for _ = 1, n do s.x = s.x + V end end
  local function sub_v (s, n) for _ = 1, n do s.x = s.x - V end end
  local function mul_v (s, n) for _ = 1, n do s.x = s.x * V end end
  local function band_v (s, n) for _ = 1, n do s.x = s.x & V end end
  local function shl_v (s, n) for _ = 1, n do s.x = s.x << V end end
  for name, f in pairs{ add = add_v, sub = sub_v, mul = mul_v,
                        band = band_v, shl = shl_v } do
    local w = ffi.new("FI")
    w.x, w.y = 10, 77
    jit.off()
    f(w, 200)
    local s = ffi.new("FI")
    s.x, s.y = 10, 77
    jit.flush()
    jit.opt.start("ir=0", "hotloop=1", "tint=1")
    jit.on()
    local before = jit.stats().tint_hits
    f(s, 200)
    H.check(jit.stats().tint_hits > before, name .. " field scalar did not run")
    H.eq(s.x, w.x, name .. " field scalar value")
    H.eq(s.y, 77, name .. " field scalar sibling")
  end
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("ffi/198_j2_i64_field_update ok")
