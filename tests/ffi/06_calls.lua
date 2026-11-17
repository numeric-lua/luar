-- tests/ffi/06_calls.lua -- libc calls, ffi.load, struct-by-value and
-- the deliberate unsupported cases.

local H = dofile("tests/helpers.lua")
local S = dofile("tests/ffi/support.lua")
local ffi = require "ffi"

ffi.cdef [[
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
void *memset(void *s, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
int abs(int x);
long labs(long x);
void *malloc(size_t n);
void free(void *p);
char *strerror(int errnum);
int chdir(const char *path);
int getpid(void);
int printf(const char *fmt, ...);
typedef struct { int quot; int rem; } div_t;
div_t div(int numer, int denom);

struct Pair { int a; int b; };
struct Big { char c; double d; long n; };
struct Nested { struct Pair p; int z; };
struct Bits { unsigned char a; unsigned short b; unsigned int c; };
struct PtrPair { struct Pair *p; int tag; };
union Val { int i; double d; };
struct WithArray { int a[3]; };
struct WithUnion { int t; union { int i; double d; } u; };

struct Pair pair_make(int a, int b);
int pair_sum(struct Pair p);
struct Pair pair_add(struct Pair p, struct Pair q);
int pair_sum_ptr(struct Pair *p);
void pair_bump(struct Pair *p, int d);
int pair_array_sum(struct Pair *p, int n);
struct PtrPair ptrpair_make(struct Pair *p, int tag);
int ptrpair_sum(struct PtrPair pp);
double big_sum(struct Big b);
struct Big big_make(char c, double d, long n);
struct Nested nested_make(int a, int b, int z);
int nested_sum(struct Nested n);
int bits_sum(struct Bits b);
int with_array_sum(struct WithArray w);
int with_union_sum(struct WithUnion w);
int val_int(union Val v);
union Val val_make(int i);
long long ll_add(long long a, long long b);
unsigned long long ull_add(unsigned long long a, unsigned long long b);
double d_add(double a, double b);
float f_add(float a, float b);
int fn_apply(int (*fn)(int), int x);
void sort_ints(int *arr, int n);

enum HelperColor { HC_RED = 1, HC_GREEN = 2, HC_BLUE = 3 };
enum HelperColor helper_color_next(enum HelperColor c);
int helper_color_value(enum HelperColor c);
bool helper_bool_not(bool b);
]]

local h = S.helper()

-- Plain libc calls -----------------------------------------------------
H.eq(tonumber(ffi.C.strlen("hello")), 5)
H.eq(ffi.C.strcmp("a", "b") < 0, true)
H.eq(ffi.C.strcmp("b", "a") > 0, true)
H.eq(ffi.C.strcmp("same", "same"), 0)
H.eq(ffi.C.abs(-7), 7)
H.eq(tonumber(ffi.C.labs(-9)), 9)
H.check(ffi.C.getpid() > 0)
H.check(tonumber(ffi.C.strlen("")) == 0)

local buf = ffi.new("char[8]")
ffi.C.memset(buf, 65, 4)
H.eq(ffi.string(buf, 4), "AAAA")
ffi.C.memcpy(buf, "abcd", 4)
H.eq(ffi.string(buf, 4), "abcd")

local mem = ffi.C.malloc(32)
H.check(mem ~= nil)
H.check(mem ~= nil and type(mem) == "cdata")
ffi.C.memset(mem, 0, 32)
ffi.C.free(mem)
H.check(pcall(ffi.C.free, nil))       -- free(NULL) is fine

local msg = ffi.C.strerror(2)
H.check(type(msg) == "cdata")
H.check(#ffi.string(msg) > 0)

local r
ffi.errno(0)
r = ffi.C.chdir("/definitely/not/a/real/luar/path")
H.eq(r, -1)
H.eq(ffi.errno(), 2)                  -- ENOENT on Linux
ffi.errno(0)

-- Struct-by-value and pointer arguments -------------------------------
local p = ffi.new("struct Pair", {a = 2, b = 3})
H.eq(h.pair_sum(p), 5)
H.eq(h.pair_sum_ptr(p), 5)
h.pair_bump(p, 10)
H.eq(p.a, 12)
H.eq(p.b, 13)
local q = h.pair_add(p, ffi.new("struct Pair", {a = 1, b = 2}))
H.eq(q.a, 13)
H.eq(q.b, 15)
local made = h.pair_make(4, 6)
H.eq(made.a, 4)
H.eq(made.b, 6)

local arrp = ffi.new("struct Pair[3]", {{1, 2}, {3, 4}, {5, 6}})
H.eq(h.pair_array_sum(arrp, 3), 21)

local pp = h.ptrpair_make(p, 7)
H.eq(pp.tag, 7)
H.eq(pp.p.a, 12)
H.eq(h.ptrpair_sum(pp), 12 + 13 + 7)
local pp2 = ffi.new("struct PtrPair", {p = p, tag = 3})
H.eq(h.ptrpair_sum(pp2), 12 + 13 + 3)

local nested = h.nested_make(1, 2, 3)
H.eq(nested.p.a, 1)
H.eq(nested.p.b, 2)
H.eq(nested.z, 3)
H.eq(h.nested_sum(nested), 6)

local big = h.big_make(7, 2.5, 9)
H.eq(big.c, 7)
H.eq(big.d, 2.5)
H.eq(tonumber(big.n), 9)
H.near(h.big_sum(big), 18.5)

local bits = ffi.new("struct Bits", {a = 1, b = 2, c = 3})
H.eq(bits.a, 1)
H.eq(bits.b, 2)
H.eq(bits.c, 3)
H.eq(h.bits_sum(bits), 6)

-- 64-bit / floating-point arguments and returns.
H.eq(tonumber(h.ll_add(ffi.new("int64_t", 5), ffi.new("int64_t", 7))), 12)
H.eq(tostring(h.ull_add(ffi.new("uint64_t", 5),
                        ffi.new("uint64_t", 7))), "12ULL")
H.near(h.d_add(1.5, 2.5), 4.0)
H.near(h.f_add(1.5, 2.5), 4.0, 1e-6)

-- qsort (through the C helper, which uses the libc qsort and a C
-- comparator; callbacks from C into Lua are not supported).
local ints = ffi.new("int[5]", {5, 1, 4, 2, 3})
h.sort_ints(ints, 5)
H.eq(ints[0], 1)
H.eq(ints[1], 2)
H.eq(ints[2], 3)
H.eq(ints[3], 4)
H.eq(ints[4], 5)

-- Enums and _Bool cross the call boundary as fixed scalar types.
H.eq(ffi.C.HC_GREEN, 2)
H.eq(h.helper_color_value(ffi.C.HC_GREEN), 2)
H.eq(h.helper_color_next(ffi.C.HC_BLUE), 1)
H.eq(h.helper_color_value(ffi.cast("enum HelperColor", 3)), 3)
H.eq(h.helper_bool_not(true), false)
H.eq(h.helper_bool_not(false), true)

-- Function pointers passed as arguments.
H.eq(h.fn_apply(ffi.C.abs, -3), 3)
local fptr = ffi.cast("int (*)(int)", ffi.C.abs)
H.eq(fptr(-5), 5)

-- libc div() returns a struct by value.
local dv = ffi.C.div(7, 2)
H.eq(type(dv), "cdata")
H.eq(dv.quot, 3)
H.eq(dv.rem, 1)

-- ffi.load and namespaces ---------------------------------------------
local libc = ffi.load("c")
H.eq(tonumber(libc.strlen("abcd")), 4)
local dflt = ffi.load()
H.eq(tonumber(dflt.strlen("x")), 1)
H.raises(function() return ffi.load("no-such-library-for-luar-tests") end,
         "cannot load")

-- Deliberate limitations: callbacks, unions/arrays by value.  (Variadic
-- calls now work; see tests/ffi/07_varargs.lua.)
H.raises(function() return ffi.C.no_such_luar_symbol end,
         "missing declaration")
H.raises(function() return ffi.C.abs(1, 2) end, "wrong number")
H.raises(function() h.fn_apply(function() return 1 end, 1) end,
         "cannot convert")
H.raises(function() return h.val_int(ffi.new("union Val", {i = 3})) end,
         "union")
H.raises(function() return h.val_make(3) end, "union")
H.raises(function()
  return h.with_array_sum(ffi.new("struct WithArray", {a = {1, 2, 3}}))
end, "array member")
H.raises(function()
  return h.with_union_sum(ffi.new("struct WithUnion", {t = 1, i = 2}))
end, "union member")
H.raises(function() return h.pair_sum(ffi.new("struct Nested")) end,
         "cannot convert")

-- ffi.gc with a C function finalizer.
local freed = ffi.C.malloc(16)
H.check(freed ~= nil)
ffi.gc(freed, ffi.C.free)
freed = nil
collectgarbage("collect")
collectgarbage("collect")

print("ffi/06_calls ok")
