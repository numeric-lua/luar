-- jit/218_table_value_copy.lua -- table element copies of arbitrary values.
--
-- `dst[i] = src[i]` now handles strings, tables and nil holes as well as
-- numbers, for register and closure-upvalue destinations/sources.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/218_table_value_copy: skipped (no native target)")
  return
end

local N = 200
local A = {}
for i = 1, N do
  if i % 17 == 0 then
    A[i] = nil
  elseif i % 7 == 0 then
    A[i] = "s" .. i
  else
    A[i] = { v = i }
  end
end

local D = {}
local function copy (n) for i = 1, n do D[i] = A[i] end end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
copy(N)
H.check(jit.stats().tint_hits > before, "value copy did not run")
for i = 1, N do
  if A[i] == nil then H.eq(D[i], nil, "nil " .. i)
  elseif type(A[i]) == "table" then H.eq(D[i].v, i, "table " .. i)
  else H.eq(D[i], A[i], "string " .. i) end
end

-- Upvalue destination and upvalue source.
local U = {}
local function copy_up (n) for i = 1, n do U[i] = A[i] end end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
copy_up(N)
H.check(jit.stats().tint_hits > before, "upvalue destination copy did not run")
for i = 1, N do
  if A[i] == nil then H.eq(U[i], nil, "up nil " .. i)
  elseif type(A[i]) == "table" then H.eq(U[i].v, i, "up table " .. i)
  else H.eq(U[i], A[i], "up string " .. i) end
end

local E = {}
local function copy_from_up (n) for i = 1, n do E[i] = U[i] end end
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
before = jit.stats().tint_hits
copy_from_up(N)
H.check(jit.stats().tint_hits > before, "upvalue source copy did not run")
for i = 1, N do
  if U[i] == nil then H.eq(E[i], nil, "src nil " .. i)
  elseif type(U[i]) == "table" then H.eq(E[i].v, i, "src table " .. i)
  else H.eq(E[i], U[i], "src string " .. i) end
end

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/218_table_value_copy ok")
