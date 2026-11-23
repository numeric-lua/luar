-- ffi/143_j4_field_chain.lua -- cdata struct field chains under J4.
--
-- The IR recorder has a dedicated nested GETFIELD...SETFIELD scanner.  Its
-- chain probe used an uninitialized per-instruction result flag; keep the
-- read/write/array forms correct with ir=1 and ir=0.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/143_j4_field_chain: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("ffi/143_j4_field_chain: skipped (no native target)")
  return
end

ffi.cdef[[
typedef struct FCInner { int x; double y; int arr[4]; } FCInner;
typedef struct FCOuter { FCInner inner; int z; } FCOuter;
]]

local function fresh ()
  local o = ffi.new("FCOuter")
  o.inner.x = 3
  o.inner.y = 1.5
  o.z = -2
  for k = 0, 3 do o.inner.arr[k] = k + 1 end
  return o
end

local function sumx (o, n)
  local s = 0
  for i = 1, n do s = s + o.inner.x end
  return s
end

local function sumarr (o, n)
  local s = 0
  for i = 1, n do s = s + o.inner.arr[i % 4] end
  return s
end

local function setx (o, n)
  for i = 1, n do o.inner.x = o.inner.x + i end
  return o.inner.x
end

local function sety (o, n)
  local s = 0
  for i = 1, n do
    o.inner.y = o.inner.y + 0.5
    s = s + o.inner.y
  end
  return s, o.inner.y
end

local function mix (o, n)
  local s = 0
  for i = 1, n do
    s = s + o.inner.x + o.inner.arr[i % 4]
    o.z = o.z + 1
  end
  return s, o.z
end

local cases = { sumx, sumarr, setx, sety, mix }

for ci, f in ipairs(cases) do
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1")
  jit.off()
  local w1, w2 = f(fresh(), 1000)
  jit.flush()
  jit.opt.start("ir=1", "hotloop=1")
  jit.on()
  local g1, g2 = f(fresh(), 1000)
  H.eq(g1, w1, "case " .. ci .. ": first result")
  H.eq(g2, w2, "case " .. ci .. ": second result")
end

-- A nested scalar read is the shape the IR root recorder supports.  Use a
-- fresh Proto so earlier interpreter runs cannot affect hot counters.
do
  local f = assert(load([[
    return function (o, n)
      local s = 0
      for i = 1, n do s = s + o.inner.x end
      return s
    end
  ]], "=fc_nested_read"))()
  jit.flush()
  jit.opt.start("ir=1", "hotloop=1")
  jit.on()
  H.eq(f(fresh(), 1000), 3000, "nested scalar read")
  H.check(jit.stats().ir_compiled >= 1, "nested field read was not recorded")
end

jit.flush()
jit.opt.start("ir=0", "hotloop=56")
jit.on()
print("ffi/143_j4_field_chain ok")
