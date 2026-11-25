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

-- Attribute combinations and packed-struct interaction.
ffi.cdef[[
#pragma pack(push,1)
typedef struct { char a; int b __attribute__((aligned(4))); char c; } FP;
#pragma pack(pop)
typedef struct { char a; int b __attribute__((packed, aligned(4))); char c; } FC1;
typedef struct { char a; int b __attribute__((aligned(4), packed)); char c; } FC2;
typedef struct { char a; int b:4 __attribute__((aligned(8))); int c:4; } FBF;
]]
H.eq(ffi.sizeof("FP"), 6, "packed struct + aligned field size")
H.eq(ffi.offsetof("FP", "b"), 1, "packed struct + aligned field offset")
H.eq(ffi.alignof("FP"), 1, "packed struct alignment")
H.eq(ffi.sizeof("FC1"), 12, "packed,aligned size")
H.eq(ffi.offsetof("FC1", "b"), 4, "packed,aligned offset")
H.eq(ffi.alignof("FC1"), 4, "packed,aligned alignment")
H.eq(ffi.sizeof("FC2"), 12, "aligned,packed size")
H.eq(ffi.offsetof("FC2", "b"), 4, "aligned,packed offset")
H.eq(ffi.sizeof("FBF"), 16, "aligned bitfield size")
H.eq(ffi.alignof("FBF"), 8, "aligned bitfield alignment")

print("ffi/197_field_attributes combinations ok")
