-- semantics/errors.lua -- error propagation and the 5.5 nil-error rule.

local H = dofile("tests/helpers.lua")

local ok, err = pcall(function() error(nil) end)
H.check(not ok, "error(nil) must fail")
H.eq(type(err), "string")            -- 5.5: nil error becomes a string
H.eq(err, "<no error object>")

ok, err = pcall(function() error({ code = 7 }) end)
H.check(not ok)
H.eq(type(err), "table")
H.eq(err.code, 7)

-- error levels add position information
ok, err = pcall(function() error("boom", 2) end)
H.check(not ok and err:find("boom"))

-- xpcall handler gets the error and its result is returned
ok, err = xpcall(function() error("inner") end, function(e) return "handled:" .. e end)
H.check(not ok)
H.check(err:find("handled:"), err)

-- pcall passes through results
ok, err = pcall(function() return 1, 2, 3 end)
H.check(ok)
H.eq(err, 1)

-- runtime errors are catchable
ok, err = pcall(function() local t = nil; return t.x end)
H.check(not ok and err:find("index"), tostring(err))

-- assert returns its arguments
H.eq(select("#", assert(1, 2, 3)), 3)

-- a __call chain deeper than 15 is rejected (5.5 limit)
local callable = function() return "ok" end
for _ = 1, 10 do
  callable = setmetatable({}, { __call = callable })
end
H.eq(callable(), "ok")

for _ = 1, 10 do
  callable = setmetatable({}, { __call = callable })
end
local cok, cerr = pcall(callable)
H.check(not cok, "20-deep __call chain must be rejected")
H.check(tostring(cerr):find("chain too long"), tostring(cerr))

print("semantics/errors ok")
