-- tests/ffi/02_cdef.lua -- recursive-descent parser coverage and errors.

local H = dofile("tests/helpers.lua")
local ffi = require "ffi"

ffi.cdef [[
/* block comment */
// line comment
typedef unsigned char u8;
typedef const volatile unsigned int cv_uint;

typedef struct Node {
  int value;
  struct Node *next;
} Node;

typedef struct {
  int x, y;
  char name[8];
  struct { short s; unsigned char b; } sub;
} Multi;

struct Outer {
  int tag;
  union { int i; double d; };
  struct { int a; int b; };
};

enum Color { RED, GREEN = 5, BLUE, };
typedef enum { FLAG_A = 1 << 2, FLAG_B = FLAG_A + 1, FLAG_C } Flags;
enum { MASK = (1 << 3) | 1 };

struct Fwd;
struct UsesFwd { struct Fwd *p; };
struct Fwd { int x; };

typedef int SixInts[BLUE];
typedef int (*BinOp)(int a, int b);
int (*callback_var)(int, void *);
int sum(int a, int b);
const char *name_of ( void );
int ga, *gb, gc[3];
]]

H.eq(ffi.sizeof("Node"), 16)
H.eq(ffi.offsetof("Node", "value"), 0)
H.eq(ffi.offsetof("Node", "next"), 8)
H.eq(ffi.sizeof("u8"), 1)
H.eq(ffi.sizeof("cv_uint"), 4)

H.eq(ffi.sizeof("Multi"), 20)
H.eq(ffi.offsetof("Multi", "x"), 0)
H.eq(ffi.offsetof("Multi", "y"), 4)
H.eq(ffi.offsetof("Multi", "name"), 8)
H.eq(ffi.offsetof("Multi", "sub"), 16)

H.eq(ffi.sizeof("struct Outer"), 24)
H.eq(ffi.offsetof("struct Outer", "tag"), 0)
H.eq(ffi.offsetof("struct Outer", "i"), 8)
H.eq(ffi.offsetof("struct Outer", "d"), 8)
H.eq(ffi.offsetof("struct Outer", "a"), 16)
H.eq(ffi.offsetof("struct Outer", "b"), 20)

H.eq(ffi.sizeof("enum Color"), 4)
H.eq(ffi.C.RED, 0)
H.eq(ffi.C.GREEN, 5)
H.eq(ffi.C.BLUE, 6)
H.eq(ffi.C.FLAG_A, 4)
H.eq(ffi.C.FLAG_B, 5)
H.eq(ffi.C.FLAG_C, 6)
H.eq(ffi.C.MASK, 9)
H.eq(ffi.sizeof("Flags"), 4)
H.eq(ffi.sizeof("SixInts"), 24)
H.eq(ffi.sizeof("BinOp"), 8)
H.eq(ffi.sizeof("int (*)(int, int)"), 8)

H.eq(ffi.sizeof("struct UsesFwd"), 8)
H.eq(ffi.sizeof("struct Fwd"), 4)
H.eq(ffi.sizeof("int[?]", 7), 28)
H.eq(ffi.alignof("int[?]"), 4)
ffi.cdef("struct BitOnly { int x : 3; };")
H.eq(ffi.sizeof("struct BitOnly"), 4)

-- Errors must be clear and name the offending token.
local function bad (src, needle)
  local ok, err = pcall(ffi.cdef, src)
  H.check(not ok, "cdef unexpectedly succeeded: " .. src)
  err = tostring(err)
  H.check(err:find(needle, 1, true) ~= nil,
          "error for '" .. src .. "' lacks '" .. needle .. "': " .. err)
  H.check(err:find("near", 1, true) ~= nil,
          "error for '" .. src .. "' lacks offending token: " .. err)
end

bad("int f(int x) __attribute__((unused));", "unsupported C construct")
bad("struct __attribute__((packed)) S { int x; };", "unsupported C construct")
bad("struct B { int x : 0; };", "width")
bad("long double d;", "long double")
bad("int x", "expected ';'")
bad("struct C { int; };", "does not declare")
bad("int f(,);", "expected a C type")
bad("...", "expected a C type")
bad("/* unterminated", "unterminated comment")
bad("int x = ;", "constant expression")

print("ffi/02_cdef ok")
