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

-- mixed base types are rejected rather than laid out incorrectly
H.raises(function ()
  return ffi.cdef("struct BitMix { unsigned char a : 4; unsigned int b : 12; };")
end, "different base types")

-- bitfield structs are accepted for field access but not by-value calls
H.raises(function () return h.bitpack_by_value(b) end, "bitfield")

print("ffi/13_bitfields ok")
