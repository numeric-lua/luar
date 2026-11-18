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
struct PackedS packedmake2(int a, int b);
struct PackedS packedmake3(int a, double b, int c);
struct PackedS packedswap(struct PackedS p);
struct PackedS packedadd(struct PackedS a, struct PackedS b);
struct PackedS packedwideadd(struct PackedWide a, struct PackedWide b);
struct PackedWide packedwideid(struct PackedWide p);
struct PackedS packedmix(struct PackedS p, int k);
struct PackedS packedmixf(struct PackedS p, double d);
struct PackedS packedmix2(struct PackedS p, int a, double b);
struct PackedS packedmixmulti(struct PackedS a, struct PackedS b, int k);
struct PackedS packedmixmulti2(struct PackedS a, double d, struct PackedS b, int k);
struct PackedS packedadd4(struct PackedS a, struct PackedS b, struct PackedS c, struct PackedS d);
void set_packed_cb(int (*cb)(struct PackedS));
void set_packed_ret_cb(struct PackedS (*cb)(struct PackedS));
void set_wide_ret_cb(struct PackedWide (*cb)(struct PackedWide));
void set_double_cb(double (*cb)(struct PackedS));
double invoke_packed_d(struct PackedS p);
void set_packed2_cb(int (*cb)(struct PackedS, struct PackedS));
int invoke_stored2(void);
void set_packed2_ret_cb(struct PackedS (*cb)(struct PackedS, struct PackedS));
struct PackedS invoke_stored2_ret(void);
void set_mix_cb(int (*cb)(struct PackedS, int));
int invoke_stored_mix(void);
void set_mix_ret_cb(struct PackedS (*cb)(struct PackedS, int));
struct PackedS invoke_stored_mix_ret(void);
void set_mix2_cb(int (*cb)(struct PackedS, struct PackedS, int));
int invoke_stored_mix2(void);
void set_mixf_cb(int (*cb)(struct PackedS, double));
int invoke_stored_mixf(void);
int invoke_packed(struct PackedS p);
struct PackedS invoke_packed_ret(struct PackedS p);
struct PackedWide invoke_wide(struct PackedWide p);

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
local made2a = h.packedmake2(10, 32)
H.eq(made2a.c, 10)
H.eq(made2a.i, 42)
H.eq(made2a.s, 32)
local made3 = h.packedmake3(5, 2.5, 7)
H.eq(made3.c, 5)
H.eq(made3.i, 14)
H.eq(made3.s, 7)
local sw = h.packedswap(p)
H.eq(sw.c, 99)
H.eq(sw.i, 123457)
H.eq(sw.s, 7)
local p2 = ffi.new("struct PackedS", {c = 2, i = 1000, s = 11})
local sum = h.packedadd(p, p2)
H.eq(sum.c, 9)
H.eq(sum.i, 124456)
H.eq(sum.s, 110)
local pw2 = ffi.new("struct PackedWide", {a = 5, b = 200, c = 4})
local wsum = h.packedwideadd(pw, pw2)
H.eq(wsum.c, 7)
H.eq(wsum.i, 15)
H.eq(wsum.s, 1200)
local wid = h.packedwideid(pw)
H.eq(wid.a, 10)
H.eq(wid.b, 1000)
H.eq(wid.c, 3)
local mx = h.packedmix(p, 1000)
H.eq(mx.i, 124456)
local mf = h.packedmixf(p, 1000.5)
H.eq(mf.i, 124456)
local m2 = h.packedmix2(p, 1000, 0.5)
H.eq(m2.i, 124456)
local mm = h.packedmixmulti(p, p2, 100)
H.eq(mm.i, 123456 + 1000 + 100)
local mm2 = h.packedmixmulti2(p, 2.5, p2, 100)
H.eq(mm2.i, 123456 + 1000 + 100 + 2)
local pa = h.packedadd4(p, p2, p2, p)
H.eq(pa.i, 123456 + 1000 + 1000 + 123456)

local cb = ffi.callback("int (*)(struct PackedS)",
  function (x) return x.i end)
h.set_packed_cb(cb)
H.eq(h.invoke_packed(p), 123456)
cb:set(function (x) return x.i + 1 end)
H.eq(h.invoke_packed(p), 123457)
local cb2 = ffi.callback("struct PackedS (*)(struct PackedS)",
  function (x) x.i = x.i + 1; return x end)
h.set_packed_ret_cb(cb2)
local cbr = h.invoke_packed_ret(p)
H.eq(cbr.i, 123457)
local cbw = ffi.callback("struct PackedWide (*)(struct PackedWide)",
  function (x) x.b = x.b + 1; return x end)
h.set_wide_ret_cb(cbw)
local cbwr = h.invoke_wide(pw)
H.eq(cbwr.b, 1001)
local cbd = ffi.callback("double (*)(struct PackedS)",
  function (x) return x.i end)
h.set_double_cb(cbd)
H.eq(h.invoke_packed_d(p), 123456.0)
local cbtmp = ffi.callback("int (*)(struct PackedS)", function () return 0 end)
cbtmp:free()
local cb2p = ffi.callback("int (*)(struct PackedS, struct PackedS)",
  function (a, b) return a.i + b.i end)
h.set_packed2_cb(cb2p)
H.eq(h.invoke_stored2(), 120)
local cb2pr = ffi.callback("struct PackedS (*)(struct PackedS, struct PackedS)",
  function (a, b) a.i = a.i + b.i; return a end)
h.set_packed2_ret_cb(cb2pr)
local c2r = h.invoke_stored2_ret()
H.eq(c2r.i, 120)
local cbmix = ffi.callback("int (*)(struct PackedS, int)",
  function (a, k) return a.i + k end)
h.set_mix_cb(cbmix)
H.eq(h.invoke_stored_mix(), 1100)
local cbmixr = ffi.callback("struct PackedS (*)(struct PackedS, int)",
  function (a, k) a.i = a.i + k; return a end)
h.set_mix_ret_cb(cbmixr)
local cbmixrres = h.invoke_stored_mix_ret()
H.eq(cbmixrres.i, 1100)
local cbmix2 = ffi.callback("int (*)(struct PackedS, struct PackedS, int)",
  function (a, b, k) return a.i + b.i + k end)
h.set_mix2_cb(cbmix2)
H.eq(h.invoke_stored_mix2(), 1120)
local cbmixf = ffi.callback("int (*)(struct PackedS, double)",
  function (a, d) return a.i + math.floor(d) end)
h.set_mixf_cb(cbmixf)
H.eq(h.invoke_stored_mixf(), 102)

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
