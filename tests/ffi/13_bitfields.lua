-- ffi/13_bitfields.lua -- bitfield layout, get/set and initializers.
local ffi = require "ffi"
local H = dofile("tests/helpers.lua")
local S = dofile("tests/ffi/support.lua")

ffi.cdef[[
struct BitPack { unsigned int a : 3; int b : 5; unsigned int c : 8; int d; };
int size_BitPack(void);
int off_BitPack_d(void);
int bitpack_a_ptr(const struct BitPack *p);
int bitpack_b_ptr(const struct BitPack *p);
int bitpack_c_ptr(const struct BitPack *p);
int bitpack_d_ptr(const struct BitPack *p);
int bitpack_by_value(struct BitPack p);
struct BitMix { unsigned char a : 4; unsigned int b : 12;
                unsigned char c : 4; int d; };
int size_BitMix(void);
int off_BitMix_d(void);
int bitmix_a_ptr(const struct BitMix *p);
int bitmix_b_ptr(const struct BitMix *p);
int bitmix_c_ptr(const struct BitMix *p);
struct BitAfter { char x; int a : 3; };
int size_BitAfter(void);
int bafter_a_ptr(const struct BitAfter *p);
struct BitLong { char a : 4; long b : 40; char c; };
int size_BitLong(void);
int off_BitLong_c(void);
long blong_b_ptr(const struct BitLong *p);
struct ZeroBit { char a; int : 0; char b; };
int size_ZeroBit(void);
int off_ZeroBit_b(void);
struct PadBit { unsigned a : 3; unsigned : 2; unsigned b : 3; };
int size_PadBit(void);
unsigned padbit_a_ptr(const struct PadBit *p);
unsigned padbit_b_ptr(const struct PadBit *p);
]]

local h = S.helper()
H.eq(ffi.sizeof("struct BitPack"), h.size_BitPack())
H.eq(ffi.offsetof("struct BitPack", "d"), h.off_BitPack_d())

local b = ffi.new("struct BitPack")
b.a = 7
b.b = -3
b.c = 200
b.d = 42
H.eq(b.a, 7)
H.eq(b.b, -3)
H.eq(b.c, 200)
H.eq(b.d, 42)

local p = ffi.cast("struct BitPack *", b)
H.eq(h.bitpack_a_ptr(p), 7)
H.eq(h.bitpack_b_ptr(p), -3)
H.eq(h.bitpack_c_ptr(p), 200)
H.eq(h.bitpack_d_ptr(p), 42)

-- positional and named initializers go through the bitfield setter
local n = ffi.new("struct BitPack", 5, -2, 9, 11)
H.eq(n.a, 5)
H.eq(n.b, -2)
H.eq(n.c, 9)
H.eq(n.d, 11)
local t = ffi.new("struct BitPack", {a = 1, b = 3, c = 255, d = -7})
H.eq(t.a, 1)
H.eq(t.b, 3)
H.eq(t.c, 255)
H.eq(t.d, -7)

-- values are masked to the field width
b.a = 9
H.eq(b.a, 1)
b.b = 20
H.eq(b.b, -12)

-- mixed base types share GCC storage units
H.eq(ffi.sizeof("struct BitMix"), h.size_BitMix())
H.eq(ffi.offsetof("struct BitMix", "d"), h.off_BitMix_d())
local m = ffi.new("struct BitMix", {a = 9, b = 3000, c = 15, d = 8})
local mp = ffi.cast("struct BitMix *", m)
H.eq(m.a, 9)
H.eq(m.b, 3000)
H.eq(m.c, 15)
H.eq(m.d, 8)
H.eq(h.bitmix_a_ptr(mp), 9)
H.eq(h.bitmix_b_ptr(mp), 3000)
H.eq(h.bitmix_c_ptr(mp), 15)

-- bitfields may continue in the same unit after a normal field
local q = ffi.new("struct BitAfter", {x = 1, a = 3})
local qp = ffi.cast("struct BitAfter *", q)
H.eq(ffi.sizeof("struct BitAfter"), h.size_BitAfter())
H.eq(q.x, 1)
H.eq(q.a, 3)
H.eq(h.bafter_a_ptr(qp), 3)

-- a wide bitfield crossing byte boundaries
local w = ffi.new("struct BitLong", {a = 7, b = 0x123456789, c = 7})
local wp = ffi.cast("struct BitLong *", w)
H.eq(ffi.sizeof("struct BitLong"), h.size_BitLong())
H.eq(ffi.offsetof("struct BitLong", "c"), h.off_BitLong_c())
H.eq(w.a, 7)
H.eq(w.b, 0x123456789)
H.eq(w.c, 7)
H.eq(h.blong_b_ptr(wp), 0x123456789)

-- unnamed zero-width and padding bitfields
H.eq(ffi.sizeof("struct ZeroBit"), h.size_ZeroBit())
H.eq(ffi.offsetof("struct ZeroBit", "b"), h.off_ZeroBit_b())
local zb = ffi.new("struct ZeroBit", {a = 1, b = 2})
H.eq(zb.a, 1)
H.eq(zb.b, 2)
local pb = ffi.new("struct PadBit", {a = 5, b = 6})
local pbp = ffi.cast("struct PadBit *", pb)
H.eq(ffi.sizeof("struct PadBit"), h.size_PadBit())
H.eq(pb.a, 5)
H.eq(pb.b, 6)
H.eq(h.padbit_a_ptr(pbp), 5)
H.eq(h.padbit_b_ptr(pbp), 6)

-- bitfield structs are accepted for field access but not by-value calls
H.raises(function () return h.bitpack_by_value(b) end, "bitfield")

-- union bitfields share offset zero
ffi.cdef[[
union BitU { unsigned a:3; unsigned b:5; };
unsigned bitu_b(union BitU u);
]]
local bu = ffi.new("union BitU")
bu.a = 5
H.eq(bu.a, 5)
H.eq(bu.b, 5)
bu.b = 17
H.eq(bu.a, 1)
H.eq(bu.b, 17)
H.eq(h.bitu_b(bu), 17)
H.eq(ffi.sizeof("union BitU"), 4)

print("ffi/13_bitfields ok")
