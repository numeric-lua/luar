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

-- two-level scalar field reads are folded into one CGETFIELD
ffi.cdef[[struct luar_trace_Inner { int x; double y; };
          struct luar_trace_Outer { struct luar_trace_Inner inner; };]]
local no = ffi.new("struct luar_trace_Outer")
no.inner.x = 3
local function nf (o, n)
  local s = 0
  for i = 1, n do s = s + o.inner.x end
  return s
end
assert(nf(no, 1000) == 3000)
assert(jit.stats().ir_executed >= 9)

-- two-level scalar field writes fold into one CSETFIELD/CSETFIELDK
local nw = ffi.new("struct luar_trace_Outer")
local function nfw (o, n)
  for i = 1, n do o.inner.x = i end
end
local function nfk (o, n)
  for i = 1, n do o.inner.y = 2.5 end
end
nfw(nw, 1000)
nfk(nw, 1000)
assert(nw.inner.x == 1000)
assert(nw.inner.y == 2.5)

-- three-level scalar reads fold into one CGETFIELD
ffi.cdef[[struct luar_trace_C { int x; double y; };
          struct luar_trace_B { struct luar_trace_C c; };
          struct luar_trace_A { struct luar_trace_B b; };]]
local ao = ffi.new("struct luar_trace_A")
ao.b.c.x = 7
local function af (o, n)
  local s = 0
  for i = 1, n do s = s + o.b.c.x end
  return s
end
assert(af(ao, 1000) == 7000)

-- three-level scalar writes fold into one CSETFIELD/CSETFIELDK
local aw = ffi.new("struct luar_trace_A")
local function awf (o, n)
  for i = 1, n do o.b.c.x = i end
end
local function awk (o, n)
  for i = 1, n do o.b.c.y = 2.5 end
end
awf(aw, 1000)
awk(aw, 1000)
assert(aw.b.c.x == 1000)
assert(aw.b.c.y == 2.5)

-- pointer-field elements fold into CPGETI/CPGETF
ffi.cdef[[struct luar_trace_Ptr { int *p; double *q; };]]
local n = 1000
local ai = ffi.new("int[?]", n)
local ad = ffi.new("double[?]", n)
local ps = ffi.new("struct luar_trace_Ptr")
for i = 0, n - 1 do ai[i] = i + 1; ad[i] = i + 0.5 end
ps.p = ai
ps.q = ad
local function pfi (s, n)
  local x = 0
  for i = 0, n - 1 do x = x + s.p[i] end
  return x
end
local function pff (s, n)
  local x = 0.0
  for i = 0, n - 1 do x = x + s.q[i] end
  return x
end
assert(pfi(ps, n) == 500500)
assert(pff(ps, n) == 500000.0)

-- pointer-field writes use CPSETI/CPSETF
local wps = ffi.new("struct luar_trace_Ptr")
wps.p = ffi.new("int[?]", n)
wps.q = ffi.new("double[?]", n)
local function pwi (s, n)
  for i = 0, n - 1 do s.p[i] = i end
end
local function pwd (s, n, x)
  for i = 0, n - 1 do s.q[i] = x end
end
pwi(wps, n)
pwd(wps, n, 1.5)
assert(wps.p[n - 1] == n - 1)
assert(wps.q[n - 1] == 1.5)

-- 8-byte integer pointer elements use CPGETI64/CPSETI64
ffi.cdef[[struct luar_trace_LongPtr { long *lp; };]]
local la = ffi.new("long[?]", n)
local lps = ffi.new("struct luar_trace_LongPtr")
lps.lp = la
local function lpw (s, n)
  for i = 0, n - 1 do s.lp[i] = i end
end
local function lpr (s, n)
  local x = 0
  for i = 0, n - 1 do x = x + s.lp[i] end
  return x
end
lpw(lps, n)
assert(tonumber(lpr(lps, n)) == 499500)

-- computed pointer indexes fold an ADDI into the pointer-field read
local pidx = ffi.new("struct luar_trace_Ptr")
local ia = ffi.new("int[?]", n + 1)
for i = 0, n do ia[i] = i end
pidx.p = ia
local function pif (s, n)
  local x = 0
  for i = 0, n - 1 do x = x + s.p[i + 1] end
  return x
end
assert(pif(pidx, n) == 500500)

-- cdata pointer arithmetic folds into the pointer-field read
local par = ffi.new("struct luar_trace_Ptr")
par.p = ia
local function paf (s, n)
  local x = 0
  for i = 0, n - 1 do x = x + (s.p + i)[0] end
  return x
end
assert(paf(par, n) == 499500)

-- pointer-arithmetic writes use CPSETI/CPSETF
local paw = ffi.new("struct luar_trace_Ptr")
paw.p = ffi.new("int[?]", n)
paw.q = ffi.new("double[?]", n)
local function pawf (s, n)
  for i = 0, n - 1 do (s.p + i)[0] = i end
end
local function pawd (s, n, x)
  for i = 0, n - 1 do (s.q + i)[0] = x end
end
pawf(paw, n)
pawd(paw, n, 1.5)
assert(paw.p[n - 1] == n - 1)
assert(paw.q[n - 1] == 1.5)

-- constant-offset pointer arithmetic: (p + i + 1)[0]
local poff = ffi.new("struct luar_trace_Ptr")
local oa = ffi.new("int[?]", n + 1)
for i = 0, n do oa[i] = i end
poff.p = oa
local function pof (s, n)
  local x = 0
  for i = 0, n - 2 do x = x + (s.p + i + 1)[0] end
  return x
end
assert(pof(poff, 1001) == 500500)

-- two-step pointer offset read: (p + i + 1 + 1)[0]
local p2 = ffi.new("struct luar_trace_Ptr")
local p2a = ffi.new("int[?]", 1003)
for i = 0, 1002 do p2a[i] = i end
p2.p = p2a
local function p2f (s)
  local x = 0
  for i = 0, 1000 do x = x + (s.p + i + 1 + 1)[0] end
  return x
end
assert(p2f(p2) == 502502)

-- constant-offset pointer writes (integer path)
local pow = ffi.new("struct luar_trace_Ptr")
pow.p = ffi.new("int[?]", n + 1)
local function powf (s, n)
  for i = 0, n - 2 do (s.p + i + 1)[0] = i end
end
powf(pow, 1001)
assert(pow.p[1000] == 999)

-- constant-offset double pointer writes
local powd = ffi.new("struct luar_trace_Ptr")
powd.q = ffi.new("double[?]", n + 1)
local function powdf (s, n, x)
  for i = 0, n - 2 do (s.q + i + 1)[0] = x end
end
powdf(powd, 1001, 1.5)
assert(powd.q[1000] == 1.5)
assert(jit.stats().ir_executed >= 27)

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
