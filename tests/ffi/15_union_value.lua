-- ffi/15_union_value.lua -- small scalar unions by value.
local ffi = require "ffi"
local H = dofile("tests/helpers.lua")
local S = dofile("tests/ffi/support.lua")

ffi.cdef[[
union Val15 { int i; double d; };
int val_int(union Val15 v);
union Val15 val_make(int i);
union DoubleU15 { double d; };
double du_get(union DoubleU15 u);
union DoubleU15 du_make(double d);
union FloatU15 { float f; };
float fu_get(union FloatU15 u);
union FloatU15 fu_make(float f);
union MixedU15 { float f; int i; };
int mu_get(union MixedU15 u);
union MixedU15 mu_make(int i);
typedef union CbVal { int i; double d; } CbVal;
int call_val_cb(CbVal (*cb)(void));
union PairU16 { double d[2]; };
double pairu_sum(union PairU16 u);
union PairU16 pairu_make(double a, double b);
union LongPairU16 { long v[2]; };
long lpair_sum(union LongPairU16 u);
union LongPairU16 lpair_make(long a, long b);
struct PairUBox16 { int tag; union PairU16 u; };
double pairu_box_sum(struct PairUBox16 b);
union Mixed16 { struct { long l; double d; } s; double d[2]; };
double mixed16_get(union Mixed16 u);
union Mixed16 mixed16_make(long l, double d);
struct Mixed16Box { int tag; union Mixed16 u; };
double mixed16_box_get(struct Mixed16Box b);
union BigD { double d[4]; };
double bigd_sum(union BigD u);
union BigD bigd_make(double a, double b, double c, double d);
union Big3D { double d[3]; };
double big3d_sum(union Big3D u);
union BigL { long v[4]; };
long bigl_sum(union BigL u);
struct BigDBox { int tag; union BigD u; };
double bigd_box_sum(struct BigDBox b);
union Char32 { char c[32]; };
int char32_sum(union Char32 u);
union Odd9 { char c[9]; };
int odd9_sum(union Odd9 u);
union Odd10 { short s[5]; };
int odd10_sum(union Odd10 u);
union Odd12 { float f[3]; };
float odd12_sum(union Odd12 u);
union Odd12 odd12_make(float a, float b, float c);
union Odd12I { int v[3]; };
int odd12i_sum(union Odd12I u);
struct Odd12Box { int tag; union Odd12 u; };
float odd12_box_sum(struct Odd12Box b);
union BigCharU { char c[8192]; };
int bigchar_sum(union BigCharU u);
union Big600 { char c[600000]; };
int big600_sum(union Big600 u);
]]

local h = S.helper()

local v = ffi.new("union Val15", {i = 7})
H.eq(h.val_int(v), 7)
H.eq(h.val_make(9).i, 9)

local d = ffi.new("union DoubleU15")
d.d = 2.5
H.eq(h.du_get(d), 2.5)
H.eq(h.du_make(3.5).d, 3.5)

local f = ffi.new("union FloatU15")
f.f = 1.25
H.eq(h.fu_get(f), 1.25)
H.eq(h.fu_make(0.5).f, 0.5)

local m = ffi.new("union MixedU15")
m.i = 42
H.eq(h.mu_get(m), 42)
H.eq(h.mu_make(17).i, 17)

local cb = ffi.callback("union CbVal(void)", function ()
  return ffi.new("union CbVal", {i = 6})
end)
H.eq(h.call_val_cb(cb), 6)

-- 16-byte unions of floats and integers
local pu = ffi.new("union PairU16", {{1.5, 2.5}})
H.eq(h.pairu_sum(pu), 4.0)
H.eq(h.pairu_make(3.0, 4.0).d[1], 4.0)
local lu = ffi.new("union LongPairU16", {{10, 20}})
H.eq(h.lpair_sum(lu), 30)
H.eq(h.lpair_make(5, 6).v[0], 5)
local box = ffi.new("struct PairUBox16", {tag = 1, u = pu})
H.eq(h.pairu_box_sum(box), 5.0)

-- a mixed INTEGER/SSE union keeps each eightbyte's class
local mx = ffi.new("union Mixed16")
mx.s.l = 3
mx.s.d = 4.5
H.eq(h.mixed16_get(mx), 7.5)
H.eq(h.mixed16_make(10, 2.5).s.l, 10)
local mxb = ffi.new("struct Mixed16Box", {tag = 1, u = mx})
H.eq(h.mixed16_box_get(mxb), 8.5)

-- larger unions go through the memory ABI path
local bd = ffi.new("union BigD", {{1.0, 2.0, 3.0, 4.0}})
H.eq(h.bigd_sum(bd), 10.0)
H.eq(h.bigd_make(1.0, 1.5, 2.0, 2.5).d[3], 2.5)
local b3 = ffi.new("union Big3D", {{1.0, 2.0, 3.0}})
H.eq(h.big3d_sum(b3), 6.0)
local bl = ffi.new("union BigL", {{1, 2, 3, 4}})
H.eq(h.bigl_sum(bl), 10)
local bb = ffi.new("struct BigDBox", {tag = 1, u = bd})
H.eq(h.bigd_box_sum(bb), 11.0)
local c32 = ffi.new("union Char32")
for i = 0, 31 do c32.c[i] = 1 end
H.eq(h.char32_sum(c32), 32)

-- large memory-class unions beyond the old 4096-field cap
local bigu = ffi.new("union BigCharU")
H.eq(ffi.sizeof("union BigCharU"), 8192)
bigu.c[0] = 2
bigu.c[8191] = 3
H.eq(h.bigchar_sum(bigu), 5)
local big600 = ffi.new("union Big600")
H.eq(ffi.sizeof("union Big600"), 600000)
big600.c[0] = 1
big600.c[599999] = 2
H.eq(h.big600_sum(big600), 3)

-- odd sizes 9..15 use alignment-sized fields
local o9 = ffi.new("union Odd9")
for i = 0, 8 do o9.c[i] = 1 end
H.eq(h.odd9_sum(o9), 9)
local o10 = ffi.new("union Odd10")
for i = 0, 4 do o10.s[i] = i + 1 end
H.eq(h.odd10_sum(o10), 15)
local o12 = ffi.new("union Odd12", {{1.0, 2.0, 3.0}})
H.eq(h.odd12_sum(o12), 6.0)
H.eq(h.odd12_make(1.5, 2.5, 3.0).f[2], 3.0)
local o12i = ffi.new("union Odd12I", {{2, 3, 4}})
H.eq(h.odd12i_sum(o12i), 9)
local o12b = ffi.new("struct Odd12Box", {tag = 1, u = o12})
H.eq(h.odd12_box_sum(o12b), 7.0)

print("ffi/15_union_value ok")
