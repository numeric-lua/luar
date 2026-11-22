-- ffi/100_packed_union_bits.lua -- packed union bitfields are only as wide
-- as the bits they store, matching the C compiler.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/100_packed_union_bits: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")
local S = dofile("tests/ffi/support.lua")

ffi.cdef[[
union __attribute__((packed)) PackedBitU {
  unsigned char a : 3;
  unsigned int b : 17;
};
int size_PackedBitU(void);
int align_PackedBitU(void);
unsigned packedbitu_b_ptr(const union PackedBitU *p);
union __attribute__((packed)) PackedBitWide {
  unsigned long long a : 40;
  unsigned char c;
};
int size_PackedBitWide(void);
int align_PackedBitWide(void);
unsigned long long packedbitwide_a_ptr(const union PackedBitWide *p);
]]
local h = S.helper()

H.eq(ffi.sizeof("union PackedBitU"), h.size_PackedBitU())
H.eq(ffi.alignof("union PackedBitU"), h.align_PackedBitU())
H.eq(ffi.sizeof("union PackedBitWide"), h.size_PackedBitWide())
H.eq(ffi.alignof("union PackedBitWide"), h.align_PackedBitWide())

-- The two fields overlap at bit zero; b spans three bytes.
local u = ffi.new("union PackedBitU")
u.b = 100000
H.eq(u.b, 100000)
H.eq(u.a, 100000 & 7)
H.eq(h.packedbitu_b_ptr(u), 100000)
u.a = 5
H.eq(u.a, 5)
H.eq(h.packedbitu_b_ptr(u), 100000 & ~7 | 5)

local w = ffi.new("union PackedBitWide")
w.a = 0x123456789
H.eq(w.a, 0x123456789)
H.eq(h.packedbitwide_a_ptr(w), 0x123456789)
H.eq(w.c, 0x89)  -- low byte overlaps 'c'

print("ffi/100_packed_union_bits ok")
