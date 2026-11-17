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

print("ffi/15_union_value ok")
