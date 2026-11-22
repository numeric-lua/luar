-- ffi/101_zero_width_bitfields.lua -- zero-width bitfields advance to the
-- base type's natural boundary even inside a packed struct or #pragma pack.
--
-- The pack attribute only packs real members; a zero-width bitfield still
-- starts the next field on an aligned boundary.  Ignoring that made packed
-- structs with a trailing or interior `: 0` too small and put following
-- fields at the wrong offset.
local ffi = require "ffi"
local H = dofile("tests/helpers.lua")
local S = dofile("tests/ffi/support.lua")

ffi.cdef[[
struct ZWInner { int x; char y; };
struct ZWPack1 {
  void *p;
  unsigned char a : 1;
  unsigned char b : 2;
  short c;
  short : 0;
} __attribute__((packed));
struct ZWPack2 {
  short a : 4;
  long long : 0;
  unsigned char b : 3;
  struct ZWInner inner;
  char c;
} __attribute__((packed));
#pragma pack(push, 1)
struct ZWPack3 {
  short a : 4;
  long long : 0;
  unsigned char b : 3;
  char c;
};
#pragma pack(pop)
struct ZWPack4 {
  struct ZWInner s;
  char a : 1;
  unsigned int : 0;
  unsigned long long b : 9;
  struct ZWInner t;
} __attribute__((packed));
struct ZWPack5 { char a; long long : 0; char b; } __attribute__((packed));
struct ZWPack6 { char a; long long : 0; char b; };
struct ZWPack7 { char a; short : 0; } __attribute__((packed, aligned(16)));
int size_ZWPack1(void); int align_ZWPack1(void); int off_ZWPack1_c(void);
int size_ZWPack2(void); int off_ZWPack2_inner(void); int off_ZWPack2_c(void);
int size_ZWPack3(void); int off_ZWPack3_c(void);
int size_ZWPack4(void); int off_ZWPack4_t(void);
int size_ZWPack5(void); int off_ZWPack5_b(void);
int size_ZWPack6(void); int off_ZWPack6_b(void);
int size_ZWPack7(void); int align_ZWPack7(void);
]]

local h = S.helper()

H.eq(ffi.sizeof("struct ZWPack1"), h.size_ZWPack1(), "ZWPack1 size")
H.eq(ffi.alignof("struct ZWPack1"), h.align_ZWPack1(), "ZWPack1 align")
H.eq(ffi.offsetof("struct ZWPack1", "c"), h.off_ZWPack1_c(), "ZWPack1.c")

H.eq(ffi.sizeof("struct ZWPack2"), h.size_ZWPack2(), "ZWPack2 size")
H.eq(ffi.offsetof("struct ZWPack2", "inner"), h.off_ZWPack2_inner(),
     "ZWPack2.inner")
H.eq(ffi.offsetof("struct ZWPack2", "c"), h.off_ZWPack2_c(), "ZWPack2.c")

H.eq(ffi.sizeof("struct ZWPack3"), h.size_ZWPack3(), "ZWPack3 size")
H.eq(ffi.offsetof("struct ZWPack3", "c"), h.off_ZWPack3_c(), "ZWPack3.c")

H.eq(ffi.sizeof("struct ZWPack4"), h.size_ZWPack4(), "ZWPack4 size")
H.eq(ffi.offsetof("struct ZWPack4", "t"), h.off_ZWPack4_t(), "ZWPack4.t")

H.eq(ffi.sizeof("struct ZWPack5"), h.size_ZWPack5(), "ZWPack5 size")
H.eq(ffi.offsetof("struct ZWPack5", "b"), h.off_ZWPack5_b(), "ZWPack5.b")
H.eq(ffi.sizeof("struct ZWPack6"), h.size_ZWPack6(), "ZWPack6 size")
H.eq(ffi.offsetof("struct ZWPack6", "b"), h.off_ZWPack6_b(), "ZWPack6.b")
H.eq(ffi.sizeof("struct ZWPack7"), h.size_ZWPack7(), "ZWPack7 size")
H.eq(ffi.alignof("struct ZWPack7"), h.align_ZWPack7(), "ZWPack7 align")

-- The fields after a zero-width bitfield must be readable/writable at the
-- offsets the C compiler chose.
local p1 = ffi.new("struct ZWPack1")
p1.p = ffi.cast("void *", 0x1234)
p1.a = 1
p1.b = 3
p1.c = -2
H.eq(tonumber(ffi.cast("uintptr_t", p1.p)), 0x1234)
H.eq(p1.a, 1)
H.eq(p1.b, 3)
H.eq(p1.c, -2)

local p2 = ffi.new("struct ZWPack2")
p2.a = -3
p2.inner.x = 77
p2.c = 9
H.eq(p2.a, -3)
H.eq(p2.inner.x, 77)
H.eq(p2.c, 9)

local p4 = ffi.new("struct ZWPack4")
p4.s.x = 11
p4.t.x = 22
p4.t.y = 33
H.eq(p4.s.x, 11)
H.eq(p4.t.x, 22)
H.eq(p4.t.y, 33)

local p5 = ffi.new("struct ZWPack5")
p5.a = 4
p5.b = 5
H.eq(p5.a, 4)
H.eq(p5.b, 5)

print("ffi/101_zero_width_bitfields ok")
