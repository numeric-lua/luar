-- tests/ffi/22_trace_cdata.lua -- J4 can read integer cdata array cells.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/22_trace_cdata: skipped (FFI unavailable)")
  return
end
if jit.status() ~= true then
  print("ffi/22_trace_cdata: skipped (no native target)")
  return
end

local a = ffi.new("int[?]", 1001)
for i = 0, 1000 do a[i] = i end

jit.flush()
jit.opt.start("ir=1")
jit.on()
local function sum (a, n)
  local s = 0
  for i = 1, n do s = s + a[i] end
  return s
end
assert(sum(a, 1000) == 500500)
assert(jit.stats().ir_executed >= 1)

-- integer cdata writes use CSETI
local w = ffi.new("int[?]", 1001)
local function fill (w, n)
  local s = 0
  for i = 1, n do
    w[i] = i
    s = s + w[i]
  end
  return s
end
assert(fill(w, 1000) == 500500)
assert(w[1000] == 1000)

-- double cdata arrays use the CGETF float specialization
local d = ffi.new("double[?]", 1001)
for i = 0, 1000 do d[i] = i + 0.5 end
local function sumf (d, n)
  local s = 0
  for i = 1, n do s = s + d[i] end
  return s
end
assert(sumf(d, 1000) == 501000.0)
assert(jit.stats().ir_executed >= 2)

-- scalar struct fields use CGETFIELD
ffi.cdef[[struct luar_trace_P { int x; double y; };]]
local sp = ffi.new("struct luar_trace_P")
sp.x = 2
sp.y = 1.5
local function sfx (p, n)
  local s = 0
  for i = 1, n do s = s + p.x end
  return s
end
local function sfy (p, n)
  local s = 0.0
  for i = 1, n do s = s + p.y end
  return s
end
assert(sfx(sp, 1000) == 2000)
assert(sfy(sp, 1000) == 1500.0)

-- scalar struct writes use CSETFIELD
local wp = ffi.new("struct luar_trace_P")
local wq = ffi.new("struct luar_trace_P")
wq.y = 2.5
local function wfx (p, n)
  for i = 1, n do p.x = i end
end
local function wfy (p, q, n)
  for i = 1, n do p.y = q.y end
end
wfx(wp, 1000)
wfy(wp, wq, 1000)
assert(wp.x == 1000)
assert(wp.y == 2.5)

-- constant integer field writes use CSETFIELDK
local kp = ffi.new("struct luar_trace_P")
local function wfk (p, n)
  for i = 1, n do p.x = 7 end
end
wfk(kp, 1000)
assert(kp.x == 7)

-- constant double field writes use CSETFIELDK
local fp = ffi.new("struct luar_trace_P")
local function wff (p, n)
  for i = 1, n do p.y = 2.5 end
end
wff(fp, 1000)
assert(fp.y == 2.5)
assert(jit.stats().ir_executed >= 8)

-- double cdata writes use CSETF
local cw = ffi.new("double[?]", 1001)
local function copy (dst, src, n)
  local s = 0
  for i = 1, n do
    dst[i] = src[i]
    s = s + dst[i]
  end
  return s
end
assert(copy(cw, d, 1000) == 501000.0)
assert(cw[1000] == 1000.5)

jit.flush()
jit.opt.start("ir=0")
jit.on()
print("ffi/22_trace_cdata ok")
