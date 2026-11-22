-- ffi/103_wide_bitfield_span.lua -- a 64-bit bitfield that starts at a
-- non-zero bit offset spans nine bytes.  Loading or storing it must not
-- read or write past the field (the accessor used to memcpy a ninth byte
-- through a uint64_t temporary, which ASan reports as a stack overflow).
local ffi = require "ffi"
local H = dofile("tests/helpers.lua")
local S = dofile("tests/ffi/support.lua")

ffi.cdef[[
struct WideBit {
  unsigned char a : 1;
  unsigned long long b : 64;
} __attribute__((packed));
struct WideBitS {
  unsigned char a : 1;
  long long b : 64;
} __attribute__((packed));
struct WideBit3 {
  unsigned char a : 7;
  unsigned long long b : 64;
  unsigned char c;
} __attribute__((packed));
int size_WideBit(void);
int align_WideBit(void);
int size_WideBitS(void);
int size_WideBit3(void);
int off_WideBit3_c(void);
unsigned long long widebit_b(struct WideBit *p);
void widebit_set_b(struct WideBit *p, unsigned long long v);
long long widebits_b(struct WideBitS *p);
int widebit3_c(struct WideBit3 *p);
]]

local h = S.helper()
local function hx (s)
  return (s:gsub(".", function (c) return string.format("%02x", c:byte()) end))
end

H.eq(ffi.sizeof("struct WideBit"), h.size_WideBit())
H.eq(ffi.alignof("struct WideBit"), h.align_WideBit())
H.eq(ffi.sizeof("struct WideBit"), 9)
H.eq(ffi.sizeof("struct WideBitS"), h.size_WideBitS())
H.eq(ffi.sizeof("struct WideBit3"), h.size_WideBit3())
H.eq(ffi.offsetof("struct WideBit3", "c"), h.off_WideBit3_c())

local off, bit, bits = ffi.offsetof("struct WideBit", "b")
H.eq(off, 0)
H.eq(bit, 1)
H.eq(bits, 64)

local allones = ffi.new("uint64_t", -1)
local w = ffi.new("struct WideBit")
w.a = 1
w.b = allones
H.eq(w.a, 1)
H.eq(w.b, -1)
H.eq(h.widebit_b(ffi.cast("struct WideBit *", w)), -1)
H.eq(hx(ffi.string(ffi.cast("char *", w), 9)), "ffffffffffffffff01")

-- Writing the field must preserve the preceding bits and the following
-- member exactly.
local wide3 = ffi.new("struct WideBit3")
wide3.a = 0x7f
wide3.b = allones
wide3.c = 0x5a
H.eq(wide3.a, 0x7f)
H.eq(wide3.b, -1)
H.eq(wide3.c, 0x5a)
H.eq(h.widebit3_c(ffi.cast("struct WideBit3 *", wide3)), 0x5a)
H.eq(hx(ffi.string(ffi.cast("char *", wide3), 10)),
     "ffffffffffffffff7f5a")

wide3.b = 0
H.eq(wide3.a, 0x7f)
H.eq(wide3.b, 0)
H.eq(wide3.c, 0x5a)
H.eq(hx(ffi.string(ffi.cast("char *", wide3), 10)),
     "7f00000000000000005a")

-- Signed 64-bit fields at bit offset 1 must round-trip both -1 and the
-- minimum integer value.
local ws = ffi.new("struct WideBitS")
ws.a = 1
ws.b = -1
H.eq(ws.b, -1)
H.eq(h.widebits_b(ffi.cast("struct WideBitS *", ws)), -1)
ws.b = math.mininteger
H.eq(ws.b, math.mininteger)
H.eq(h.widebits_b(ffi.cast("struct WideBitS *", ws)), math.mininteger)
H.eq(ws.a, 1)

-- The initializer path goes through the same setter.
local wi = ffi.new("struct WideBit", {a = 1, b = allones})
H.eq(wi.a, 1)
H.eq(wi.b, -1)

print("ffi/103_wide_bitfield_span ok")
