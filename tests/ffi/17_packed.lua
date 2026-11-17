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
struct PackedWide { int a; long b; char c; } __attribute__((packed));
long packedwide_sum(struct PackedWide p);
struct PackedS packedmake0(void);
struct PackedS packedmake(int i);
struct PackedS packedmakef(double d);

#pragma pack(push, 1)
struct PragmaPacked { char c; int i; short s; };
#pragma pack(pop)
int size_PragmaPacked(void);
int off_PragmaPacked_i(void);
int pragmapacked_i_ptr(const struct PragmaPacked *p);
#pragma pack(push, 2)
struct Pack2 { char c; int i; };
#pragma pack(pop)
int size_Pack2(void);
int off_Pack2_i(void);
struct PackNatural { char c; int i; };
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

-- single packed integer arguments are passed through a MEMORY-ABI thunk
H.eq(h.packed_by_value(p), 123456)
local pref = ffi.cast("struct PackedS *", p)[0]
H.eq(h.packed_by_value(pref), 123456)
local pw = ffi.new("struct PackedWide", {a = 10, b = 1000, c = 3})
H.eq(h.packedwide_sum(pw), 1013)
local made = h.packedmake0()
H.eq(made.c, 5)
H.eq(made.i, 777)
H.eq(made.s, 9)
local made2 = h.packedmake(4242)
H.eq(made2.c, 1)
H.eq(made2.i, 4242)
H.eq(made2.s, 2)
local madef = h.packedmakef(9.5)
H.eq(madef.c, 2)
H.eq(madef.i, 9)
H.eq(madef.s, 3)

-- #pragma pack applies the same byte-granular layout
H.eq(ffi.sizeof("struct PragmaPacked"), h.size_PragmaPacked())
H.eq(ffi.offsetof("struct PragmaPacked", "i"), h.off_PragmaPacked_i())
local pp = ffi.new("struct PragmaPacked")
pp.c = 2
pp.i = 777
pp.s = 3
H.eq(h.pragmapacked_i_ptr(pp), 777)
H.eq(ffi.sizeof("struct Pack2"), h.size_Pack2())
H.eq(ffi.offsetof("struct Pack2", "i"), h.off_Pack2_i())
H.eq(ffi.offsetof("struct PackNatural", "i"), 4)  -- pack popped

print("ffi/17_packed ok")
