-- jit/171_while_temp_writes.lua -- the fast while peephole must not drop an
-- `i*i` temporary when a second accumulator operation is selected.
--
-- The emitter writes fw_temp only in the square/linear forms.  With
-- `x = i*i` plus `s = s*i` or `s = s-i`/`s = s/i` it chose the second
-- operation and left x at its entry value.  Such bodies must fall back to
-- the generic branch emitter.
local H = dofile("tests/helpers.lua")

if jit.status() ~= true then
  print("jit/171_while_temp_writes: skipped (no native target)")
  return
end

local function run (name, src)
  local f = assert(load(src, "=whiletemp"))()
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "table=0", "tint=1")
  jit.off()
  local want = { pcall(f, 300) }
  jit.flush()
  jit.opt.start("ir=0", "hotloop=1", "table=0", "tint=1")
  jit.on()
  local got = { pcall(f, 300) }
  H.eq(#got, #want, name .. ": result count")
  H.eq(got[1], want[1], name .. ": error status")
  for i = 2, #want do
    H.eq(got[i], want[i], name .. ": result " .. i)
  end
end

run("temp+product", [[
return function (n)
  local x, s, i = 0, 1, 1
  while i <= n do
    x = i * i
    s = s * i
    i = i + 1
  end
  return x, s, i
end]])

run("temp+sub", [[
return function (n)
  local x, s, i = 0, 0, 1
  while i <= n do
    x = i * i
    s = s - i
    i = i + 1
  end
  return x, s, i
end]])

run("temp+div", [[
return function (n)
  local x, s, i = 0, 1.0, 1
  while i <= n do
    x = i * i
    s = s / i
    i = i + 1
  end
  return x, s, i
end]])

-- The square form is representable and must stay correct.
run("temp+square", [[
return function (n)
  local x, s, i = 0, 0, 1
  while i <= n do
    x = i * i
    s = s + x
    i = i + 1
  end
  return x, s, i
end]])

-- A linear float temporary followed by a second operation is also safe
-- only if the emitter writes the temporary.
run("temp+linear", [[
return function (n)
  local x, s, i = 0, 0.0, 1
  while i <= n do
    x = i * 0.5
    s = s + x
    i = i + 1
  end
  return x, s, i
end]])

print("jit/171_while_temp_writes ok")
