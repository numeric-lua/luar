-- ffi/149_new_pointer_string.lua -- ffi.new only accepts a string for a
-- character pointer destination.
--
-- LuaJIT rejects `ffi.new("void *", "abc")`; a void * is not a character
-- type, and silently reinterpreting the bytes hides a type error.  The
-- documented Luar convenience for char * (and the other 1-byte character
-- spellings) is preserved, as are ffi.cast and string arguments in calls.
local ok, ffi = pcall(require, "ffi")
if not ok then
  print("ffi/149_new_pointer_string: skipped (FFI unavailable)")
  return
end
local H = dofile("tests/helpers.lua")

-- Character pointer destinations keep the convenience.
H.eq(ffi.string(ffi.new("char *", "abc")), "abc", "char *")
H.eq(ffi.string(ffi.new("const char *", "abc")), "abc", "const char *")
H.eq(ffi.string(ffi.new("signed char *", "abc")), "abc", "signed char *")
H.eq(ffi.string(ffi.new("unsigned char *", "abc")), "abc", "unsigned char *")

-- void * and non-character pointers reject a string initializer.
H.raises(function () return ffi.new("void *", "abc") end, "cannot convert")
H.raises(function () return ffi.new("int *", "abc") end, "cannot convert")
H.raises(function () return ffi.new("double *", "abc") end, "cannot convert")

-- NULL and pointer/aggregate initializers still work for void *.
local null = ffi.new("void *", nil)
H.check(null == nil, "void * nil is NULL")
local a = ffi.new("int[2]", {1, 2})
H.check(ffi.new("void *", a) ~= nil, "array initializes void *")
H.raises(function () return ffi.new("void *", ffi.new("int", 3)) end,
         "cannot convert")

-- ffi.cast remains permissive for any pointer type.
H.eq(ffi.string(ffi.cast("void *", "abc")), "abc", "cast void *")

-- String arguments in C calls still convert to const void * parameters.
ffi.cdef[[
void *memchr(const void *s, int c, size_t n);
]]
local p = ffi.C.memchr("abc", string.byte("b"), 3)
H.check(p ~= nil, "memchr found the byte")
H.eq(ffi.string(ffi.cast("char *", p), 1), "b")

print("ffi/149_new_pointer_string ok")
