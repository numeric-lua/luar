-- ffi/177_sizeof_adjacent.lua -- sizeof of adjacent string literals.
--
-- C concatenates adjacent string literals before sizing them.  The
-- constant-expression parser now accepts `sizeof("a" "bc")` and counts
-- escaped bytes in every fragment before adding the trailing NUL.
local H = dofile("tests/helpers.lua")
local ffi = require("ffi")

ffi.cdef[[
_Static_assert(sizeof("a" "bc") == 4, "two fragments");
_Static_assert(sizeof("a" "\n") == 3, "escape fragment");
_Static_assert(sizeof("" "" "") == 1, "empty fragments");
_Static_assert(sizeof("a" "" "b") == 3, "empty middle");
enum { ADJ = sizeof("hello" "!"), ADJ_NL = sizeof("a" "\n" "b") };
typedef struct { char data[sizeof("x" "y")]; } NAdj;
]]

H.eq(ffi.C.ADJ, 7, "adjacent enum size")
H.eq(ffi.C.ADJ_NL, 4, "adjacent escape size")
H.eq(ffi.sizeof("NAdj"), 3, "adjacent array member")
H.eq(ffi.sizeof("char[sizeof(\"a\" \"bc\")]"), 4, "adjacent nested sizeof")

-- A non-string token after the fragments is still an error.
H.raises(function ()
  ffi.cdef[[ _Static_assert(sizeof("a" 1) == 2, "bad"); ]]
end, "expected")

print("ffi/177_sizeof_adjacent ok")
