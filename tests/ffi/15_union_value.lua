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

print("ffi/15_union_value ok")
