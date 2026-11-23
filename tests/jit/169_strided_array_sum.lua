-- jit/169_strided_array_sum.lua -- raw-array reductions with non-unit steps.
--
-- `for i = 1, n, 2 do s = s + a[i] end` and the downward form are common
-- strided kernels.  The intrinsics used to reject every step other than 1
-- and fall back to the interpreter.  They now walk the same key sequence as
-- FORLOOP, including negative steps, while still checking every visited
-- element and preserving the observable element/product registers.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/169_strided_array_sum: skipped (no native target)")
  return
end

local function bits (x)
  return string.pack("d", x)
end

local function same (a, b)
  if math.type(a) ~= math.type(b) then return false end
  if math.type(a) == "float" then
    if a ~= a and b ~= b then return true end
    return bits(a) == bits(b)
  end
  return a == b
end

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "table=0", "tint=1")

local N = 401
local A, B, C, UP = {}, {}, {}, {}
for i = 1, N do
  A[i] = i * 5 - 3
  B[i] = i * 2 + 1
  C[i] = i - 100
  UP[i] = i + 0.25
end

local function run (f, a, b, c, n)
  jit.flush()
  jit.off()
  local want = { pcall(f, a, b, c, n) }
  jit.flush()
  jit.on()
  local before = jit.stats().tint_hits
  local got = { pcall(f, a, b, c, n) }
  local st = jit.stats()
  H.eq(#got, #want, "result count")
  H.eq(got[1], want[1], "error status")
  if got[1] then
    for i = 2, #want do
      H.check(same(got[i], want[i]),
              "result " .. i .. " differs: " .. tostring(got[i]) ..
              " ~= " .. tostring(want[i]))
    end
  else
    H.eq(tostring(got[2]), tostring(want[2]), "error message")
  end
  return st, before
end

local function sum_step (a, b, c, n)
  local s = 0
  for i = 1, n, 2 do s = s + a[i] end
  return s
end

local function sum_step3 (a, b, c, n)
  local s = 0
  for i = 2, n, 3 do s = s + a[i] end
  return s
end

local function sum_down (a, b, c, n)
  local s = 0
  for i = n, 1, -2 do s = s + a[i] end
  return s
end

local function sum_res (a, b, c, n)
  local s = 0
  local x
  for i = 1, n, 3 do x = a[i]; s = s + x end
  return s, x
end

local function pair_step (a, b, c, n)
  local s = 0
  for i = 1, n, 2 do s = s + a[i] + b[i] end
  return s
end

local function pair_down (a, b, c, n)
  local s = 0
  for i = n, 1, -3 do s = s + a[i] - b[i] end
  return s
end

local function dot_step (a, b, c, n)
  local s = 0
  for i = 1, n, 2 do s = s + a[i] * b[i] end
  return s
end

local function three_step (a, b, c, n)
  local s = 0
  for i = 1, n, 2 do s = s + a[i] + b[i] + c[i] end
  return s
end

local function three_down (a, b, c, n)
  local s = 0
  for i = n, 1, -2 do s = s + a[i] + b[i] + c[i] end
  return s
end

local function three_up (a, b, c, n)
  local s = 0
  for i = 1, n, 3 do s = s + a[i] + UP[i] + c[i] end
  return s
end

local function scale_step (a, b, c, n)
  local s = 0
  for i = 1, n, 2 do s = s + a[i] * 3 end
  return s
end

local function scale_down (a, b, c, n)
  local s = 0.5
  for i = n, 1, -2 do s = s + a[i] * 0.25 end
  return s
end

local st, before = run(sum_step, A, B, C, N)
H.check(st.tint_hits > before, "step-2 sum did not use the intrinsic")
st, before = run(sum_step3, A, B, C, N)
H.check(st.tint_hits > before, "step-3 sum did not use the intrinsic")
st, before = run(sum_down, A, B, C, N)
H.check(st.tint_hits > before, "negative-step sum did not use the intrinsic")
st, before = run(sum_res, A, B, C, N)
H.check(st.tint_hits > before, "strided element register did not use the intrinsic")
st, before = run(pair_step, A, B, C, N)
H.check(st.tint_hits > before, "strided pair sum did not use the intrinsic")
st, before = run(pair_down, A, B, C, N)
H.check(st.tint_hits > before, "downward pair difference did not use the intrinsic")
st, before = run(dot_step, A, B, C, N)
H.check(st.tint_hits > before, "strided dot product did not use the intrinsic")
st, before = run(three_step, A, B, C, N)
H.check(st.tint_hits > before, "strided three-array sum did not use the intrinsic")
st, before = run(three_down, A, B, C, N)
H.check(st.tint_hits > before, "downward three-array sum did not use the intrinsic")
st, before = run(three_up, A, B, C, N)
H.check(st.tint_hits > before, "mixed strided three-array sum did not use the intrinsic")
st, before = run(scale_step, A, B, C, N)
H.check(st.tint_hits > before, "strided scaled sum did not use the intrinsic")
st, before = run(scale_down, A, B, C, N)
H.check(st.tint_hits > before, "downward scaled sum did not use the intrinsic")

-- A non-visited bad element must not disable the intrinsic; a visited one
-- must still raise the interpreter's error.
local sparse = {}
for i = 1, N do sparse[i] = i end
sparse[2] = "not visited by step 2"
local function odd_sum (a, b, c, n)
  local s = 0
  for i = 1, n, 2 do s = s + sparse[i] end
  return s
end
local ok, err = pcall(odd_sum, A, B, C, N)
H.check(ok, "a non-visited bad element broke the strided intrinsic: " ..
             tostring(err))
sparse[3] = "visited"
H.raises(function () return odd_sum(A, B, C, N) end, "attempt to add",
         "visited bad element")

-- Endpoint bounds: a step past the end runs fewer iterations, not a
-- truncated or overrun array walk.
local function step_big (a, b, c, n)
  local s = 0
  for i = 1, n, 1000 do s = s + a[i] end
  return s
end
run(step_big, A, B, C, N)

local short = {}
for i = 1, N - 1 do short[i] = i end
run(sum_step, short, B, C, N)

local meta = setmetatable({}, { __index = function (_, k) return k end })
for i = 1, N do rawset(meta, i, i) end
run(sum_step, meta, B, C, N)

-- Wrap and float promotion stay exact on strided runs too.
local function step_float (a, b, c, n)
  local s = 0
  for i = 1, n, 2 do s = s + UP[i] end
  return s
end
run(step_float, A, B, C, N)

local big = {}
for i = 1, N do big[i] = math.maxinteger - i end
local function step_wrap (a, b, c, n)
  local s = 0
  for i = 1, n, 2 do s = s + big[i] end
  return s
end
run(step_wrap, A, B, C, N)

jit.flush()
jit.opt.start("hotloop=56", "ir=0", "table=0", "tint=0")
print("jit/169_strided_array_sum ok")
