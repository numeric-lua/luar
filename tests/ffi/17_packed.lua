-- ffi/17_packed.lua -- __attribute__((packed)) layout and access.
local ffi = require "ffi"
local H = dofile("tests/helpers.lua")
local S = dofile("tests/ffi/support.lua")

ffi.cdef[[
struct PackedS { char c; int i; short s; } __attribute__((packed));
int size_PackedS(void);
int off_PackedS_i(void);
int off_PackedS_s(void);
int packed_i_ptr(const struct PackedS *p);
int packed_by_value(struct PackedS p);
]]
local h = S.helper()

H.eq(ffi.sizeof("struct PackedS"), h.size_PackedS())
H.eq(ffi.alignof("struct PackedS"), 1)
H.eq(ffi.offsetof("struct PackedS", "i"), h.off_PackedS_i())
H.eq(ffi.offsetof("struct PackedS", "s"), h.off_PackedS_s())

local p = ffi.new("struct PackedS")
p.c = 7
p.i = 123456
p.s = 99
H.eq(p.c, 7)
H.eq(p.i, 123456)
H.eq(h.packed_i_ptr(p), 123456)

-- libffi cannot represent a packed layout by value; reject it clearly.
H.raises(function () return h.packed_by_value(p) end, "layout")

print("ffi/17_packed ok")
