-- tests/ffi/03_layout.lua -- sizes/offsets against a C compiler.

local H = dofile("tests/helpers.lua")
local S = dofile("tests/ffi/support.lua")
local ffi = require "ffi"

ffi.cdef [[
struct LayoutPair { int a; int b; };
struct LayoutBig { char c; double d; long n; };
struct LayoutNested { struct LayoutPair p; int z; };
struct LayoutBits { unsigned char a; unsigned short b; unsigned int c; };
struct LayoutPtrPair { struct LayoutPair *p; int tag; };
union LayoutVal { int i; double d; };
struct LayoutWithArray { int a[3]; };
struct LayoutWithUnion { int t; union { int i; double d; } u; };
struct LayoutMulti {
  int x;
  int y;
  char name[8];
  struct { short s; unsigned char b; } sub;
};
struct LayoutOuter {
  int tag;
  union { int i; double d; };
  struct { int a; int b; };
};

int size_Pair(void); int off_Pair_b(void);
int size_Big(void); int off_Big_c(void); int off_Big_d(void);
int off_Big_n(void);
int size_Nested(void); int off_Nested_p(void); int off_Nested_z(void);
int size_Bits(void); int off_Bits_b(void); int off_Bits_c(void);
int size_PtrPair(void); int off_PtrPair_p(void); int off_PtrPair_tag(void);
int size_Val(void); int off_Val_i(void); int off_Val_d(void);
int size_WithArray(void);
int size_WithUnion(void); int off_WithUnion_u(void);
int size_Multi(void); int off_Multi_x(void); int off_Multi_y(void);
int off_Multi_name(void); int off_Multi_sub(void);
int size_Outer(void); int off_Outer_tag(void); int off_Outer_i(void);
int off_Outer_d(void); int off_Outer_a(void); int off_Outer_b(void);
]]

local h = S.helper()

local function same_size (name, fn)
  H.eq(ffi.sizeof(name), h[fn](), name .. " size")
end

local function same_offset (name, field, fn)
  H.eq(ffi.offsetof(name, field), h[fn](), name .. "." .. field)
end

same_size("struct LayoutPair", "size_Pair")
same_offset("struct LayoutPair", "b", "off_Pair_b")
same_size("struct LayoutBig", "size_Big")
same_offset("struct LayoutBig", "c", "off_Big_c")
same_offset("struct LayoutBig", "d", "off_Big_d")
same_offset("struct LayoutBig", "n", "off_Big_n")
same_size("struct LayoutNested", "size_Nested")
same_offset("struct LayoutNested", "p", "off_Nested_p")
same_offset("struct LayoutNested", "z", "off_Nested_z")
same_size("struct LayoutBits", "size_Bits")
same_offset("struct LayoutBits", "b", "off_Bits_b")
same_offset("struct LayoutBits", "c", "off_Bits_c")
same_size("struct LayoutPtrPair", "size_PtrPair")
same_offset("struct LayoutPtrPair", "p", "off_PtrPair_p")
same_offset("struct LayoutPtrPair", "tag", "off_PtrPair_tag")
same_size("union LayoutVal", "size_Val")
same_offset("union LayoutVal", "i", "off_Val_i")
same_offset("union LayoutVal", "d", "off_Val_d")
same_size("struct LayoutWithArray", "size_WithArray")
same_size("struct LayoutWithUnion", "size_WithUnion")
same_offset("struct LayoutWithUnion", "u", "off_WithUnion_u")
same_size("struct LayoutMulti", "size_Multi")
same_offset("struct LayoutMulti", "x", "off_Multi_x")
same_offset("struct LayoutMulti", "y", "off_Multi_y")
same_offset("struct LayoutMulti", "name", "off_Multi_name")
same_offset("struct LayoutMulti", "sub", "off_Multi_sub")
same_size("struct LayoutOuter", "size_Outer")
same_offset("struct LayoutOuter", "tag", "off_Outer_tag")
same_offset("struct LayoutOuter", "i", "off_Outer_i")
same_offset("struct LayoutOuter", "d", "off_Outer_d")
same_offset("struct LayoutOuter", "a", "off_Outer_a")
same_offset("struct LayoutOuter", "b", "off_Outer_b")

-- Arrays, pointer/array declarator precedence and alignments.
H.eq(ffi.sizeof("int[3][4]"), 48)
H.eq(ffi.alignof("int[3][4]"), 4)
H.eq(ffi.sizeof("int *[3]"), 24)
H.eq(ffi.sizeof("int (*)[3]"), 8)
H.eq(ffi.sizeof("char[17]"), 17)
H.eq(ffi.sizeof("struct LayoutPair[2]"), 16)
H.eq(ffi.sizeof(ffi.typeof("struct LayoutPair")), 8)

-- Variable-length arrays.
H.eq(ffi.sizeof("int[?]", 5), 20)
H.eq(ffi.alignof("int[?]"), 4)
local v = ffi.new("int[?]", 7)
H.eq(ffi.sizeof(v), 28)
H.eq(ffi.sizeof(ffi.typeof("int[?]"), 3), 12)
H.raises(function() return ffi.sizeof("int[?]") end, "variable%-length")

-- Invalid size/offset requests.
H.raises(function() return ffi.sizeof("void") end, "unknown")
ffi.cdef [[ struct LayoutNever; ]]
H.raises(function() return ffi.sizeof("struct LayoutNever") end, "unknown")
H.raises(function() return ffi.sizeof("int (int)") end, "unknown")
H.raises(function() return ffi.alignof("void") end, "unknown")
H.raises(function() return ffi.offsetof("int", "x") end, "struct or union")
H.raises(function() return ffi.offsetof("struct LayoutPair", "missing") end,
         "no member")

print("ffi/03_layout ok")
