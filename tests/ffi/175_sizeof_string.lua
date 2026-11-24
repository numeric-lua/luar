-- ffi/175_sizeof_string.lua -- sizeof("literal") in cdef constant
-- expressions.
--
-- C counts the bytes of a string literal plus its trailing NUL.  The
-- lexer keeps raw bytes, so escaped characters must be decoded before the
-- size is used by static assertions, enums and array bounds.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

ffi.cdef[[
_Static_assert(sizeof("abc") == 4, "abc");
_Static_assert(sizeof("") == 1, "empty");
_Static_assert(sizeof("a\n") == 3, "newline escape");
_Static_assert(sizeof("a\x41") == 3, "hex escape");
_Static_assert(sizeof("a\101") == 3, "octal escape");
_Static_assert(sizeof("a\\") == 3, "backslash escape");
_Static_assert(sizeof("a\"b") == 4, "quote escape");
enum { SZ_HELLO = sizeof("hello"), SZ_TWO = sizeof("ab") };
typedef struct { char data[sizeof("xy")]; } NStr;
]]

H.eq(ffi.C.SZ_HELLO, 6, "enum sizeof string")
H.eq(ffi.C.SZ_TWO, 3, "short string size")
H.eq(ffi.sizeof("NStr"), 3, "sizeof string in an array member")

-- sizeof of a type name is unchanged.
H.eq(ffi.sizeof("int"), 4, "type sizeof")
H.eq(ffi.sizeof("char[sizeof(\"abc\")]"), 4, "type array from sizeof string")

-- _Alignof needs a type, not a string literal.
H.raises(function ()
  ffi.cdef[[ _Static_assert(_Alignof("abc") == 1, "bad"); ]]
end, "Alignof")

-- Unterminated literals are still rejected by the lexer.
H.raises(function ()
  ffi.cdef[[ _Static_assert(sizeof("abc) == 4); ]]
end, "unterminated")

print("ffi/175_sizeof_string ok")
