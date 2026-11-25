-- ffi/197_field_attributes.lua -- field-level aligned/packed attributes.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

ffi.cdef[[
typedef struct { char a; int b __attribute__((aligned(8))); char c; } FA;
typedef struct { char a; int b __attribute__((packed)); char c; } FB;
typedef struct { char a; int b __attribute__((aligned(16))); char c; } FC;
typedef struct { FA inner; char tail; } FN;
]]

H.eq(ffi.sizeof("FA"), 16, "aligned size")
H.eq(ffi.offsetof("FA", "b"), 8, "aligned offset")
H.eq(ffi.offsetof("FA", "c"), 12, "aligned next offset")
H.eq(ffi.alignof("FA"), 8, "aligned struct alignment")

H.eq(ffi.sizeof("FB"), 6, "packed field size")
H.eq(ffi.offsetof("FB", "b"), 1, "packed field offset")
H.eq(ffi.offsetof("FB", "c"), 5, "packed field next offset")
H.eq(ffi.alignof("FB"), 1, "packed field alignment")

H.eq(ffi.sizeof("FC"), 32, "aligned16 size")
H.eq(ffi.offsetof("FC", "b"), 16, "aligned16 offset")
H.eq(ffi.alignof("FC"), 16, "aligned16 alignment")

H.eq(ffi.sizeof("FN"), 24, "nested aligned size")
H.eq(ffi.offsetof("FN", "tail"), 16, "nested tail offset")

local a = ffi.new("FA")
a.b = 42
a.c = 7
H.eq(a.b, 42, "aligned field store")
H.eq(a.c, 7, "aligned next store")

print("ffi/197_field_attributes ok")
