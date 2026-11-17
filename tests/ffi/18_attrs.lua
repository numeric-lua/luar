-- ffi/18_attrs.lua -- packed/aligned struct attributes.
local ffi = require "ffi"
local H = dofile("tests/helpers.lua")
local S = dofile("tests/ffi/support.lua")

ffi.cdef[[
struct AlignedS { int x; } __attribute__((aligned(16)));
int size_AlignedS(void);
int align_AlignedS(void);
struct PackAligned { char c; int i; } __attribute__((packed, aligned(2)));
int size_PackAligned(void);
int align_PackAligned(void);
]]
local h = S.helper()

H.eq(ffi.sizeof("struct AlignedS"), h.size_AlignedS())
H.eq(ffi.alignof("struct AlignedS"), h.align_AlignedS())
H.eq(ffi.sizeof("struct PackAligned"), h.size_PackAligned())
H.eq(ffi.alignof("struct PackAligned"), h.align_PackAligned())

local a = ffi.new("struct AlignedS")
a.x = 42
H.eq(a.x, 42)

local pa = ffi.new("struct PackAligned")
pa.c = 1
pa.i = 1234
H.eq(pa.c, 1)
H.eq(pa.i, 1234)

print("ffi/18_attrs ok")
