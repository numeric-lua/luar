-- tests/ffi/07_varargs.lua -- variadic C calls through libffi and the
-- default argument promotions.

local H = dofile("tests/helpers.lua")
local S = dofile("tests/ffi/support.lua")
local ffi = require "ffi"

ffi.cdef [[
int snprintf(char *buf, size_t n, const char *fmt, ...);
int printf(const char *fmt, ...);
size_t strlen(const char *s);
int abs(int x);

struct VarPair { int a; int b; };
union VarValue { int i; double d; };
enum VarColor { VC_RED = 1, VC_GREEN = 2, VC_BLUE = 3 };

int vararg_first_is_null(int tag, ...);
long long vararg_fixed_sum(struct VarPair p, enum VarColor c,
                           bool b, long long n, ...);
]]

local h = S.helper()
local buf = ffi.new("char[128]")

-- Calls snprintf and returns the formatted text plus its length.  The
-- format is a fixed argument (converted to const char * by the declared
-- prototype); the remaining values travel through the variadic part.
local function call (fmt, ...)
  local n = ffi.C.snprintf(buf, 128, fmt, ...)
  return ffi.string(buf), n
end

-- Mixed fixed and variadic arguments --------------------------------
local s, n = call("%d %s %.1f", 42, "x", 1.5)
H.eq(s, "42 x 1.5")
H.eq(n, 8)

-- printf itself resolves and is callable.  Passing an empty string keeps
-- the test output deterministic.
H.eq(ffi.C.printf("%s", ""), 0)

-- A variadic function may be called with no variadic arguments at all.
s = call("plain")
H.eq(s, "plain")

-- Lua integers: int when they fit, long long otherwise ---------------
s = call("%d %d %d", 0, -1, 2147483647)
H.eq(s, "0 -1 2147483647")
s = call("%d", -2147483648)
H.eq(s, "-2147483648")
s = call("%lld", 2147483648)
H.eq(s, "2147483648")
s = call("%lld", -2147483649)
H.eq(s, "-2147483649")
s = call("%lld", math.maxinteger)
H.eq(s, tostring(math.maxinteger))
s = call("%lld", math.mininteger)
H.eq(s, tostring(math.mininteger))

-- Lua floats become double ------------------------------------------
s = call("%.2f", 1.5)
H.eq(s, "1.50")
s = call("%g", 0.125)
H.eq(s, "0.125")

-- Strings become const char * and stay anchored for the call --------
local runtime = ("vari"):upper() .. ("adic"):lower()
s = call("[%s]", runtime .. tostring(2026))
H.eq(s, "[VARIadic2026]")
-- A second, differently sized temporary string right after the first
-- must not reuse stale storage.
s = call("%s|%s", "a" .. "bc", string.rep("z", 4))
H.eq(s, "abc|zzzz")

-- Booleans become int 0/1 --------------------------------------------
s = call("%d %d", true, false)
H.eq(s, "1 0")

-- nil becomes a NULL pointer -----------------------------------------
H.eq(h.vararg_first_is_null(0, nil), 1)
H.eq(h.vararg_first_is_null(0, buf), 0)
H.eq(h.vararg_first_is_null(0, ffi.NULL), 1)
H.eq(h.vararg_first_is_null(0, ffi.cast("void *", 0)), 1)
H.eq(h.vararg_first_is_null(0, ffi.cast("const char *", "x")), 0)

-- cdata scalars keep their width/signedness, with the C default
-- promotions applied to small integers and float ---------------------
s = call("%d", ffi.new("int8_t", -1))
H.eq(s, "-1")
s = call("%u", ffi.new("uint16_t", 65535))
H.eq(s, "65535")
s = call("%u", ffi.new("uint32_t", 4000000000))
H.eq(s, "4000000000")
s = call("%lld", ffi.new("int64_t", -5000000000))
H.eq(s, "-5000000000")
s = call("%llu", ffi.new("uint64_t", 123456789))
H.eq(s, "123456789")
s = call("%.2f", ffi.new("float", 1.5))
H.eq(s, "1.50")
s = call("%.2f", ffi.new("double", 2.25))
H.eq(s, "2.25")
s = call("%d", ffi.new("enum VarColor", 2))
H.eq(s, "2")
s = call("%d", ffi.new("bool", true))
H.eq(s, "1")

-- cdata pointers and arrays (arrays decay to an element pointer) -----
s = call("%s", ffi.new("char[6]", "hello"))
H.eq(s, "hello")
s = call("%s", ffi.cast("const char *", "world"))
H.eq(s, "world")

-- Fixed arguments keep their declared prototype types, including
-- struct-by-value, enum, bool and 64-bit integers --------------------
local total = h.vararg_fixed_sum(ffi.new("struct VarPair", {a = 2, b = 3}),
                                 ffi.C.VC_GREEN, true,
                                 ffi.new("int64_t", 5),
                                 ffi.new("int64_t", 7))
H.eq(tonumber(total), 2 + 3 + 2 + 1 + 5 + 7)

-- ffi.load namespaces use the same variadic call path ---------------
local libc = ffi.load("c")
local len = libc.snprintf(buf, 128, "%s+%d", "libc", 9)
H.eq(ffi.string(buf), "libc+9")
H.eq(len, 6)
H.eq(tonumber(libc.strlen("abcd")), 4)

-- Two consecutive variadic calls (state is rebuilt for each call) ---
local function twice ()
  local first = call("%d", 11)
  local second = call("%s", "twelve")
  return first, second
end
local first, second = twice()
H.eq(first, "11")
H.eq(second, "twelve")

-- Aggregate varargs are rejected with a clear error -----------------
local err = H.raises(function()
  return ffi.C.snprintf(buf, 128, "%d",
                        ffi.new("struct VarPair", {a = 1, b = 2}))
end, "struct.*variadic")
H.check(tostring(err):find("aggregate varargs are not portable", 1, true))
H.raises(function()
  return ffi.C.snprintf(buf, 128, "%d", ffi.new("union VarValue", {i = 3}))
end, "union.*variadic")

-- Other unsupported Lua values are rejected, never crash ------------
H.raises(function() return ffi.C.snprintf(buf, 128, "%s", {}) end,
         "cannot pass table")
H.raises(function() return ffi.C.snprintf(buf, 128, "%s", print) end,
         "cannot pass function")
H.raises(function()
  return ffi.C.snprintf(buf, 128, "%s", ffi.typeof("int"))
end, "C type object")

-- Too few arguments for the fixed part of the prototype ------------
H.raises(function() return ffi.C.snprintf(buf, 128) end,
         "expected at least 3")

-- A prototype with no fixed parameter before ... is not callable
-- (ISO C rejects it and libffi requires one fixed argument).
local nofixed = ffi.cast("int (*)(...)", ffi.C.abs)
H.raises(function() return nofixed(1) end, "without a fixed parameter")

-- Errors leave the call machinery usable.
s = call("%d", 1)
H.eq(s, "1")

print("ffi/07_varargs ok")
