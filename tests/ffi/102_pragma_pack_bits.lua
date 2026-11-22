-- ffi/102_pragma_pack_bits.lua -- #pragma pack(n) bitfield packing.
--
-- With any active #pragma pack, consecutive bitfields are packed without
-- retiring to the base type's full storage unit.  Only ordinary members
-- and the aggregate alignment are capped by n.  pack(1) already behaved
-- this way; pack(2)/pack(8) used to reserve whole base units and produce
-- oversized structs/unions.
local ffi = require "ffi"
local H = dofile("tests/helpers.lua")
local S = dofile("tests/ffi/support.lua")

ffi.cdef[[
#pragma pack(push, 2)
struct PPack2A {
  unsigned short f0 : 6;
  unsigned short f1 : 15;
  signed char f2 : 7;
};
struct PPack2B {
  unsigned int f0 : 21;
  long long f1;
  unsigned short f2 : 13;
  short f3 : 14;
  unsigned char f4 : 4;
};
union PPack2U { unsigned long long : 0; long long f1 : 23; unsigned char f2 : 4; };
#pragma pack(pop)
#pragma pack(push, 8)
struct PPack8A { char a; unsigned int b : 31; unsigned int c : 31; char d; };
struct PPack8B {
  unsigned short f0 : 6;
  unsigned short f1 : 15;
  signed char f2 : 7;
};
#pragma pack(pop)
int size_PPack2A(void); int align_PPack2A(void);
int size_PPack2B(void); int align_PPack2B(void);
int size_PPack2U(void); int align_PPack2U(void);
int size_PPack8A(void); int size_PPack8B(void);
]]

local h = S.helper()
H.eq(ffi.sizeof("struct PPack2A"), h.size_PPack2A(), "PPack2A size")
H.eq(ffi.alignof("struct PPack2A"), h.align_PPack2A(), "PPack2A align")
H.eq(ffi.sizeof("struct PPack2B"), h.size_PPack2B(), "PPack2B size")
H.eq(ffi.alignof("struct PPack2B"), h.align_PPack2B(), "PPack2B align")
H.eq(ffi.sizeof("union PPack2U"), h.size_PPack2U(), "PPack2U size")
H.eq(ffi.alignof("union PPack2U"), h.align_PPack2U(), "PPack2U align")
H.eq(ffi.sizeof("struct PPack8A"), h.size_PPack8A(), "PPack8A size")
H.eq(ffi.sizeof("struct PPack8B"), h.size_PPack8B(), "PPack8B size")

-- Values survive the tight packing.
local a = ffi.new("struct PPack2A")
a.f0 = 63
a.f1 = 32767
a.f2 = -64
H.eq(a.f0, 63)
H.eq(a.f1, 32767)
H.eq(a.f2, -64)

local b = ffi.new("struct PPack8A")
b.a = 1
b.b = 0x7fffffff
b.c = 0x12345678
b.d = 2
H.eq(b.a, 1)
H.eq(b.b, 0x7fffffff)
H.eq(b.c, 0x12345678)
H.eq(b.d, 2)

local u = ffi.new("union PPack2U")
u.f1 = -1
H.eq(tonumber(u.f1), -1)
H.eq(u.f2, 15)

print("ffi/102_pragma_pack_bits ok")
