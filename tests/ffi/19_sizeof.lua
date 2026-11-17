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
]]
local h = S.helper()

H.eq(ffi.sizeof("struct SzA"), 8)
H.eq(ffi.sizeof("struct SzB"), h.size_SzB())
H.eq(ffi.C.SZ_ENUM, 8)
H.eq(h.size_SzArr(), 16)
H.eq(ffi.sizeof("struct SzArr"), 16)

H.raises(function ()
  ffi.cdef("char bad[sizeof(void)];")
end, "incomplete")
print("ffi/19_sizeof ok")
