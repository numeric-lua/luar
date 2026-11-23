-- ffi/133_cast_string_pointer.lua -- ffi.cast accepts a Lua string for any
-- pointer type, matching LuaJIT.  ffi.new keeps its stricter initializer
-- rules, so only the cast surface changes.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/133_cast_string_pointer: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")

local s = "Luar\0cast"
local p = ffi.cast("int *", s)
H.check(p ~= nil, "int * cast from a string is non-NULL")
H.eq(ffi.cast("char *", p)[0], string.byte(s, 1), "first byte")
H.eq(ffi.cast("char *", p)[4], 0, "embedded NUL is preserved")
H.eq(ffi.cast("char *", p)[5], string.byte(s, 6), "byte after the NUL")

-- void *, const and struct pointers follow the same rule.
H.check(ffi.cast("void *", s) ~= nil, "void * cast from a string")
H.check(ffi.cast("const char *", s) ~= nil, "const char * cast")
ffi.cdef[[typedef struct CastStringS { int x; } CastStringS;]]
local sp = ffi.cast("CastStringS *", s)
H.check(sp ~= nil, "struct pointer cast from a string")
H.eq(ffi.cast("intptr_t", sp), ffi.cast("intptr_t", p),
     "struct and int pointers share the string address")

-- Function pointers are pointers too.
ffi.cdef[[int cast_string_fn(void);]]
local fp = ffi.cast("int (*)(void)", s)
H.check(fp ~= nil, "function pointer cast from a string")
H.eq(ffi.cast("intptr_t", fp), ffi.cast("intptr_t", p),
     "function and object pointers share the string address")
H.eq(ffi.cast("int (*)(void)", ffi.cast("void *", s)), fp,
     "round-trip through void * keeps the address")

-- NULL and numeric casts are unchanged.
H.eq(ffi.cast("int *", nil), nil, "nil casts to a NULL pointer")
H.eq(ffi.cast("intptr_t", ffi.cast("int *", 0)), 0, "zero pointer")

-- ffi.new still rejects non-char pointer string initialisers.
H.check(not pcall(ffi.new, "int *", s), "ffi.new int * string stays rejected")
H.check(not pcall(ffi.new, "CastStringS *", s),
        "ffi.new struct * string stays rejected")
H.check(ffi.new("char *", s) ~= nil, "ffi.new char * string convenience kept")

print("ffi/133_cast_string_pointer ok")
