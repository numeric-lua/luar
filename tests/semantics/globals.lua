-- semantics/globals.lua -- Lua 5.5 global declarations (the 'global' keyword).

local H = dofile("tests/helpers.lua")

-- global-by-default outside any declaration
X_GLOBAL = 1
H.eq(X_GLOBAL, 1)

-- a scoped global declaration switches that scope to strict mode
do
  global Y_GLOBAL
  Y_GLOBAL = 2
  H.eq(Y_GLOBAL, 2)
end
H.eq(Y_GLOBAL, 2)

-- undeclared free names are a compile error inside a global declaration
H.check(load("global Z1; Z1 = 1; return Z1")() == 1)
H.check(load("global Z2; return totally_undefined") == nil)

-- the declaration ends with its block
local f = load([[
  do
    global A
    A = 10
  end
  return A
]])
H.check(f ~= nil)
H.eq(f(), 10)

-- global<const> * declares every name read-only
H.check(load("global<const> *; return math.pi")() == math.pi)
H.check(load("global<const> *; X_READONLY = 1") == nil)

-- a later explicit declaration re-enables writing
H.check(load("global<const> *; global W; W = 1") ~= nil)

-- 'global' is always a reserved word; no legacy compat switch exists
H.check(load("local global = 1") == nil)

-- legacy math functions are gone (no LUA_COMPAT_MATHLIB)
H.check(math.pow == nil and math.atan2 == nil and math.log10 == nil)

-- for-loop control variables are read-only (5.5)
H.check(load("for i = 1, 3 do i = i + 1 end") == nil)
H.check(load("for k, v in pairs({}) do k = 1 end") == nil)
-- but shadowing them in the body is fine
H.check(load("for i = 1, 3 do local i = 5; i = i + 1 end") ~= nil)

print("semantics/globals ok")
