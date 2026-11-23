-- ffi/160_preproc_elif.lua -- #elif branches and skipped-group handling.
local H = dofile("tests/helpers.lua")
local ffi = require "ffi"

-- The first true branch wins; later branches are not evaluated.
ffi.cdef[[
#if 0
typedef int elif_never_a;
#elif 1
typedef int elif_taken_b;
#elif 0 / 0
typedef int elif_bad_expr;
#else
typedef int elif_never_c;
#endif
]]
H.eq(ffi.sizeof("elif_taken_b"), 4)
H.raises(function () return ffi.sizeof("elif_never_a") end, "expected a C type")
H.raises(function () return ffi.sizeof("elif_never_c") end, "expected a C type")
H.raises(function () return ffi.sizeof("elif_bad_expr") end, "expected a C type")

-- A first #if branch suppresses a malformed later #elif expression.
ffi.cdef[[
#if 1
typedef int elif_first_ok;
#elif 0 / 0
typedef int elif_not_reached;
#endif
]]
H.eq(ffi.sizeof("elif_first_ok"), 4)
H.raises(function () return ffi.sizeof("elif_not_reached") end, "expected a C type")

-- A false #if makes #elif the live branch; #else then stays dead.
ffi.cdef[[
#if 0
typedef int elif_chain_a;
#elif 0
typedef int elif_chain_b;
#elif 1
typedef int elif_chain_c;
#else
typedef int elif_chain_d;
#endif
]]
H.eq(ffi.sizeof("elif_chain_c"), 4)
H.raises(function () return ffi.sizeof("elif_chain_a") end, "expected a C type")
H.raises(function () return ffi.sizeof("elif_chain_b") end, "expected a C type")
H.raises(function () return ffi.sizeof("elif_chain_d") end, "expected a C type")

-- Directives in a skipped group are not parsed at all, including nested
-- conditionals with otherwise-invalid expressions and bad #include paths.
ffi.cdef[[
#define ELIF_DEFINED 1
#if 0
#include <this-file-does-not-exist.h>
#bad_directive
#pragma pack(3)
#if 1 / 0
typedef int elif_dead_nested;
#endif
#elif defined(ELIF_DEFINED) && ELIF_DEFINED
typedef int elif_live_after_skip;
#endif
]]
H.eq(ffi.sizeof("elif_live_after_skip"), 4)
H.raises(function () return ffi.sizeof("elif_dead_nested") end, "expected a C type")

-- A live #elif expression with a zero divisor is still an error.
local ok, err = pcall(ffi.cdef, [[
#if 0
typedef int elif_live_zero_a;
#elif 1 / 0
typedef int elif_live_zero_b;
#endif
]])
H.check(not ok, "zero divisor in a live #elif was accepted")
H.check(tostring(err):find("division by zero", 1, true) ~= nil, tostring(err))

-- C allows '#if(expr)' without a space and comments directly after
-- '#else'/'#endif'.  These forms must keep conditional nesting correct,
-- especially inside a skipped group.
ffi.cdef[[
#if(0)
typedef int elif_compact_a;
#elif(1)
typedef int elif_compact_b;
#else// not taken
typedef int elif_compact_c;
#endif/* endif */
]]
H.eq(ffi.sizeof("elif_compact_b"), 4)
H.raises(function () return ffi.sizeof("elif_compact_a") end, "expected a C type")
H.raises(function () return ffi.sizeof("elif_compact_c") end, "expected a C type")

ffi.cdef[[
#if 0
#if(1)
typedef int elif_compact_dead;
#endif/* inner */
#elif(1)
typedef int elif_compact_live;
#endif
]]
H.eq(ffi.sizeof("elif_compact_live"), 4)
H.raises(function () return ffi.sizeof("elif_compact_dead") end,
         "expected a C type")

-- Structural errors are reported.
H.raises(function () ffi.cdef("#elif 1\n") end, "#elif without #if")
H.raises(function () ffi.cdef("#if 0\n#else\n#elif 1\n#endif\n") end,
         "#elif after #else")
H.raises(function () ffi.cdef("#if 0\n#else\n#else\n#endif\n") end,
         "duplicate #else")

print("ffi/160_preproc_elif ok")
