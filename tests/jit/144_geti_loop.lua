-- jit/144_geti_loop.lua -- J2 folds constant-index array reads (GETI).
--
-- Lua compiles t[1] as GETI with an immediate index.  J2 only decoded
-- GETTABLE (register index), so fixed-slot array loops stayed on the
-- interpreter.  The native path guards the table, the array bound and the
-- element tag, and exits at the GETI bytecode on any mismatch.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/144_geti_loop: skipped (no native target)")
  return
end

local function bits (x) return string.pack("d", x) end

local function read1 (t, n)
  local s = 0
  for i = 1, n do s = s + t[1] end
  return s
end

local function read2 (t, n)
  local s = 0
  for i = 1, n do s = s + t[2] * 2 end
  return s
end

local function readf (t, n)
  local s = 0.0
  for i = 1, n do s = s + t[1] end
  return s
end

jit.flush()
jit.opt.start("hotloop=1", "table=1")
jit.on()

H.eq(read1({7, 9}, 2000), 14000)
H.check(jit.stats().compiled >= 1, "constant-index array read did not compile")

H.eq(read2({5, 11}, 2000), 44000)
H.check(jit.stats().compiled >= 2, "second constant-index loop did not compile")

H.eq(readf({2.5}, 2000), 5000.0)
H.check(jit.stats().compiled >= 3, "float accumulator GETI loop did not compile")

-- A float element in an integer accumulator must leave the native loop and
-- produce the interpreter's float result, not read the payload as an int.
local want = 0.0
for _ = 1, 2000 do want = want + 2.5 end
local got = read1({2.5}, 2000)
H.eq(math.type(got), "float")
H.eq(bits(got), bits(want))

-- A metatable forces the table path to exit before it touches the array.
local mt = { __index = function () return 13 end }
local t = setmetatable({[1] = 7}, mt)
H.eq(read1(t, 2000), 14000)

-- Bad element types still raise the interpreter's exact error from the
-- right bytecode; the guard must not restart the loop.
local bad = {[1] = "x"}
H.raises(function () return read1(bad, 2000) end, "attempt to add")

jit.opt.start("table=0")
jit.flush()
print("jit/144_geti_loop ok")
