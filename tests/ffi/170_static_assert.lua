-- ffi/170_static_assert.lua -- C11 static assertions in ffi.cdef.
--
-- Headers use _Static_assert (and the assert.h static_assert macro) for
-- layout and type checks.  The declaration parser must evaluate the
-- expression at cdef time, accept the optional diagnostic message, and
-- reject a false assertion with a useful error.  Assertions are otherwise
-- erased: they declare no type, member or symbol.
local ffi = require "ffi"
local H = dofile("tests/helpers.lua")

-- Basic forms, including the unprefixed C23/practice spelling.
ffi.cdef[[
_Static_assert(1, "one");
_Static_assert(sizeof(int) == 4);
static_assert(1 + 1 == 2, "arithmetic");
enum { SA_VALUE = 7 };
_Static_assert(SA_VALUE == 7, "enum constant");
_Static_assert(sizeof(char) == 1 && sizeof(double) == 8, "sizes");
_Static_assert((1 ? 2 : 1 / 0) == 2, "lazy conditional");
]]

-- Inside a struct or union body (C11 allows this).
ffi.cdef[[
struct StaticAssertS {
  int a;
  _Static_assert(sizeof(int) == 4, "int is 4 bytes");
  static_assert(1, "still here");
  double b;
};
union StaticAssertU {
  _Static_assert(1 + 1 == 2);
  int i;
  double d;
};
]]
H.eq(ffi.sizeof("struct StaticAssertS"), 16, "struct layout after assertions")
H.eq(ffi.offsetof("struct StaticAssertS", "b"), 8, "field after assertions")
H.eq(ffi.sizeof("union StaticAssertU"), 8, "union layout")

-- Object-like and function-like macros expand to assertions.
ffi.cdef[[
#define SA_CHECK(n) _Static_assert((n) > 0, "positive")
SA_CHECK(3);
#define SA_ALWAYS static_assert(1, "always")
SA_ALWAYS;
]]

-- A false assertion is an error, and the message is reported.
H.raises(function ()
  ffi.cdef[[_Static_assert(0, "deliberate failure");]]
end, "static assertion failed: deliberate failure")
H.raises(function ()
  ffi.cdef[[static_assert(2 + 2 == 5);]]
end, "static assertion failed")
H.raises(function ()
  ffi.cdef[[_Static_assert(1, );]]
end, "string literal")
H.raises(function ()
  ffi.cdef[[_Static_assert(1)]]
end, "expected ';'")
H.raises(function ()
  ffi.cdef[[_Static_assert(1 / 0, "dead")]]
end, "division by zero")

-- A failed assertion must not leave a partial declaration behind.
H.raises(function ()
  ffi.cdef[[
    _Static_assert(0, "stop");
    typedef int NeverRegistered;
  ]]
end, "static assertion failed")
H.raises(function ()
  return ffi.typeof("NeverRegistered")
end, "NeverRegistered")

print("ffi/170_static_assert ok")
