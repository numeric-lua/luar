-- ffi/19_sizeof.lua -- sizeof in cdef constant expressions.
local ffi = require "ffi"
local H = dofile("tests/helpers.lua")
local S = dofile("tests/ffi/support.lua")

ffi.cdef[[
struct SzA { char c; int i; };
struct SzB { char c[sizeof(struct SzA)]; int tail; };
enum { SZ_ENUM = sizeof(struct SzA) };
int size_SzB(void);
int size_SzArr(void);
struct SzArr { char c[sizeof(int[4])]; };
struct OffA { char c; int i; };
enum {
  OFF_I = offsetof(struct OffA, i),
  OFF_I2 = __builtin_offsetof(struct OffA, i),
  ALIGN_A = _Alignof(struct OffA),
  ALIGN_A2 = __alignof__(struct OffA)
};
int off_OffA_i(void);
int align_OffA(void);
]]
local h = S.helper()

H.eq(ffi.sizeof("struct SzA"), 8)
H.eq(ffi.sizeof("struct SzB"), h.size_SzB())
H.eq(ffi.C.SZ_ENUM, 8)
H.eq(h.size_SzArr(), 16)
H.eq(ffi.sizeof("struct SzArr"), 16)
H.eq(ffi.C.OFF_I, h.off_OffA_i())
H.eq(ffi.C.OFF_I2, h.off_OffA_i())
H.eq(ffi.C.ALIGN_A, h.align_OffA())
H.eq(ffi.C.ALIGN_A2, h.align_OffA())

H.raises(function ()
  ffi.cdef("char bad[sizeof(void)];")
end, "incomplete")
print("ffi/19_sizeof ok")
