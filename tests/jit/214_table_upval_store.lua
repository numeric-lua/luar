-- jit/214_table_upval_store.lua -- table stores whose destination array is
-- a closure upvalue.
--
-- `U[i] = src[i]` and `U[i] = nil` compile through GETUPVAL dest plus
-- SETTABLE.  The destination is re-read from the closure on every entry;
-- source upvalues, sparse nil clears and metatable fallbacks are covered.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/214_table_upval_store: skipped (no native target)")
  return
end

local function count (t)
  local n = 0
  for _ in pairs(t) do n = n + 1 end
  return n
end

jit.flush()
jit.opt.start("ir=0", "hotloop=1", "hotexit=1", "tint=1")
jit.on()

local N = 200
local S = {}
for i = 1, N do S[i] = i * 2 + 0.5 end
local U = S

local function copy_up (dst, n)
  for i = 1, n do dst[i] = S[i] end
end

local U2 = {}
local function copy_up2 (n)
  for i = 1, n do U2[i] = S[i] end
end

local function clear_up (n)
  for i = 1, n do U2[i] = nil end
end

local D1, D2 = {}, {}
jit.flush()
jit.opt.start("ir=0", "hotloop=1", "tint=1")
jit.on()
local before = jit.stats().tint_hits
copy_up(D1, N)
H.check(jit.stats().tint_hits > before, "upvalue copy did not run")
for i = 1, N do H.check(D1[i] == S[i], "upvalue copy element " .. i) end

U2 = D2
before = jit.stats().tint_hits
copy_up2(N)
H.check(jit.stats().tint_hits > before, "upvalue destination copy did not run")
for i = 1, N do H.check(D2[i] == S[i], "destination copy element " .. i) end

-- Sparse upvalue-table clear.
U2 = { [1000] = 1, [1500] = 2 }
for i = 1, N do U2[i] = i end
before = jit.stats().tint_hits
clear_up(N)
H.check(jit.stats().tint_hits > before, "upvalue nil clear did not run")
H.eq(count(U2), 2, "sparse outside keys remain")
H.eq(U2[1000], 1, "sparse key 1000")
H.eq(U2[1500], 2, "sparse key 1500")

-- Metatable destination falls back.
local seen = 0
local M = setmetatable({}, { __newindex = function () seen = seen + 1 end })
for i = 1, N do rawset(M, i, i) end
U2 = M
before = jit.stats().tint_hits
clear_up(N)
H.eq(jit.stats().tint_hits, before, "metatable clear must not run")
H.eq(seen, 0, "existing fields bypass __newindex")

jit.flush()
jit.opt.start("hotloop=56", "hotexit=10", "ir=0", "tint=1")
print("jit/214_table_upval_store ok")
