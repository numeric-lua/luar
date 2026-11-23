-- ffi/125_union_named_init.lua -- a union table initializer must honour the
-- named member it selects, not only the first member.
--
-- init_struct_pos used to stop after the first union field even when the
-- table named a later member, so `ffi.new("union { int i; double d; }",
-- {d = 1.5})` silently zeroed the union instead of setting `d` (LuaJIT
-- sets `d`).  Positional entries still initialize only the first member.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/125_union_named_init: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")

ffi.cdef[[
typedef union { int i; double d; } NamedUnion;
typedef union {
  int head;
  struct { int x; int y; } point;
  char text[8];
} AnonUnion;
typedef struct {
  int a;
  union { int b; double c; };
  int d;
} StructWithUnion;
typedef union { struct { int x; int y; }; int z; } UnionOfStruct;
]]

-- A named non-first member initializes that member.
local u = ffi.new("NamedUnion", {d = 1.5})
H.eq(u.d, 1.5, "named union member d was not initialized")
H.eq(u.i, 0, "unrelated union member was written")

-- The first named member wins, regardless of table insertion order.
u = ffi.new("NamedUnion", {i = 7, d = 1.5})
H.eq(u.i, 7, "first named union member did not win")
u = ffi.new("NamedUnion", {d = 1.5, i = 7})
H.eq(u.i, 7, "first named union member did not win (reverse order)")

-- Positional table entries still address the first member only.
u = ffi.new("NamedUnion", {[1] = 9})
H.eq(u.i, 9, "positional union initializer missed the first member")
u = ffi.new("NamedUnion", {[2] = 1.5})
H.eq(u.i, 0, "positional entry 2 initialized a non-first union member")
H.eq(u.d, 0.0, "positional entry 2 initialized a non-first union member")

-- A nested struct member selected by name initializes recursively.
local anon = ffi.new("AnonUnion", {point = {x = 3, y = 4}})
H.eq(anon.point.x, 3, "named nested struct field x")
H.eq(anon.point.y, 4, "named nested struct field y")
H.eq(anon.head, 3, "nested struct overlaps the first union member")

-- ... and a named array member can take a string initializer.
anon = ffi.new("AnonUnion", {text = "hi"})
H.eq(ffi.string(anon.text, 2), "hi", "named union array member")

-- Anonymous unions inside a struct behave the same way.
local s = ffi.new("StructWithUnion", {a = 1, c = 2.5, d = 3})
H.eq(s.a, 1)
H.eq(s.c, 2.5, "named member of an anonymous union")
H.eq(s.d, 3)

-- A union whose first member is an anonymous struct can select one of the
-- nested fields by name.
local us = ffi.new("UnionOfStruct", {y = 6})
H.eq(us.y, 6, "named field inside an anonymous struct union member")
H.eq(us.x, 0, "unselected nested field was written")
us = ffi.new("UnionOfStruct", {5, 6})
H.eq(us.x, 5, "positional nested union field x")
H.eq(us.y, 6, "positional nested union field y")

print("ffi/125_union_named_init ok")
