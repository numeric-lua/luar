-- ffi/179_runtime_offsetof_path.lua -- nested/indexed paths in the runtime
-- ffi.offsetof, matching the cdef constant-expression designators.
--
-- Direct fields keep their old behaviour.  Dotted paths walk nested
-- struct/union members and bracket indices walk fixed arrays.  Bitfield
-- results keep the three-value byte/bit/width form.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

ffi.cdef[[
typedef struct { int a; double b; } RI;
typedef struct { char c; RI i; int tail; } RO;
typedef struct { RI a[3]; int z; } RA;
typedef union { int x; double y; } RU;
typedef struct { unsigned a:3; unsigned b:5; } RB;
]]

H.eq(ffi.offsetof("RO", "i.a"), 8, "nested int")
H.eq(ffi.offsetof("RO", "i.b"), 16, "nested double")
H.eq(ffi.offsetof("RO", "tail"), 24, "direct field")
H.eq(ffi.offsetof("RA", "a[0].a"), 0, "array element 0")
H.eq(ffi.offsetof("RA", "a[1].b"), 24, "array element 1 double")
H.eq(ffi.offsetof("RA", "a[2].a"), 32, "array element 2")
H.eq(ffi.offsetof("RU", "y"), 0, "union field")

local off, bit, width = ffi.offsetof("RB", "a")
H.eq(off, 0, "bitfield byte")
H.eq(bit, 0, "bitfield position")
H.eq(width, 3, "bitfield width")
off, bit, width = ffi.offsetof("RB", "b")
H.eq(off, 0, "second bitfield byte")
H.eq(bit, 3, "second bitfield position")
H.eq(width, 5, "second bitfield width")

H.raises(function () ffi.offsetof("RO", "i.missing") end, "no member")
H.raises(function () ffi.offsetof("RO", "i.b.c") end, "no member")
H.raises(function () ffi.offsetof("RO", "i[1]") end, "array")
H.raises(function () ffi.offsetof("RO", "i[bad]") end, "array")
H.raises(function () ffi.offsetof(42, "i") end, "C type expected")

print("ffi/179_runtime_offsetof_path ok")
