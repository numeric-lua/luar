-- jit/176_field_table_sum.lua -- J2 sums a raw table array reached through a
-- constant table field (`s = s + obj.arr[i]`).
--
-- The field is re-read on every native entry and validated as a table
-- without a metatable; replacing it, shrinking the array or making the field
-- missing falls back to the interpreter.  The GETFIELD/GETTABLE temporary
-- keeps the last element like the bytecode.
local H = dofile("tests/helpers.lua")
if jit.status() ~= true then
  print("jit/176_field_table_sum: skipped (no native target)")
  return
end

local N = 257

local function make (fill)
  local t = {}
  for i = 1, N do t[i] = fill(i) end
  return t
end

local function run (name, fill, start, stop, step)
  local function f (obj, n, start, stop, step)
    local s = 0
    for i = start, stop, step do s = s + obj.arr[i] end
    return s
  end
  local obj = { arr = make(fill) }
  jit.flush()
  jit.off()
  local want = f(obj, N, start, stop, step)
  jit.flush()
  jit.on()
  local before = jit.stats().tint_hits
  local got = f(obj, N, start, stop, step)
  local hits = jit.stats().tint_hits - before
  H.eq(got, want, name .. ": sum")
  H.check(hits > 0, name .. ": did not use the field-array intrinsic")
end

run("integer field array", function (i) return i * 7 - 100 end, 1, N, 1)
run("float field array", function (i) return i / 3 end, 1, N, 1)
run("strided field array", function (i) return (i * 13) % 97 end, 1, N, 2)
run("descending field array", function (i) return i * 3 - 11 end, N, 1, -3)

-- Replacing the field after compilation must be observed.
do
  local function f (obj, n)
    local s = 0
    for i = 1, n do s = s + obj.arr[i] end
    return s
  end
  local obj = { arr = make(function (i) return i end) }
  jit.flush()
  jit.on()
  local first = f(obj, N)
  H.eq(first, N * (N + 1) // 2, "field array first value")
  obj.arr = make(function (i) return 1000 - i end)
  local want = 0
  for i = 1, N do want = want + (1000 - i) end
  H.eq(f(obj, N), want, "field array sees a replaced field")
end

-- A metatable is fine while the raw field is present; a missing field must
-- go through __index instead of the raw helper.
do
  local function f (obj, n)
    local s = 0
    for i = 1, n do s = s + obj.arr[i] end
    return s
  end
  local raw = setmetatable({ arr = make(function (i) return i * 2 end) }, {
    __index = function () error("__index must not run") end,
  })
  jit.flush()
  jit.on()
  H.eq(f(raw, N), N * (N + 1), "raw field with a metatable")

  local calls = 0
  local lazy = setmetatable({}, {
    __index = function (t, k)
      calls = calls + 1
      t[k] = make(function (i) return i end)
      return t[k]
    end,
  })
  jit.flush()
  jit.on()
  H.eq(f(lazy, N), N * (N + 1) // 2, "missing field goes through __index")
  H.eq(calls, 1, "__index should run exactly once")
end

-- A field that is not a table is not a raw array; the interpreter raises
-- the same error.
do
  local function f (obj, n)
    local s = 0
    for i = 1, n do s = s + obj.arr[i] end
    return s
  end
  local obj = { arr = 7 }
  jit.flush()
  jit.on()
  H.raises(function () f(obj, N) end, "index", "non-table field")
end
