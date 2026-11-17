-- jit/03_guards.lua -- differential checks for compiled loops and guards.
local H = dofile("tests/helpers.lua")

if jit.status() == false then
  print("jit/03_guards: skipped (no native target)")
  return
end

local function same (name, f, ...)
  local args = table.pack(...)
  jit.off()
  local want = f(table.unpack(args, 1, args.n))
  jit.flush()
  jit.on()
  local got = f(table.unpack(args, 1, args.n))
  if got ~= want and not (got ~= got and want ~= want) then
    error(string.format("%s: jit %s ~= interpreter %s", name,
                        tostring(got), tostring(want)), 2)
  end
end

local function int_sum (n, step)
  local s = 0
  for i = 1, n, step do s = s + i end
  return s
end
same("int sum", int_sum, 5000, 1)
same("int sum step", int_sum, 5000, 7)
same("int sum down", function (n)
  local s = 0
  for i = n, 1, -1 do s = s + i end
  return s
end, 5000)

same("empty loop", function ()
  local s = 0
  for i = 1, 0 do s = s + i end
  return s
end)

same("step overflow", function ()
  local s = 0
  for i = 1, 100, math.maxinteger do s = s + i end
  return s
end)

same("wrap", function ()
  local s = 0
  for i = 1, 200 do s = s + math.maxinteger end
  return s
end)

same("mul", function ()
  local p = 1
  for i = 1, 40 do p = p * 3 end
  return p
end)

same("mixed", function (a)
  local t = 0
  for i = 1, 5000 do t = t + a * i end
  return t
end, 2)
same("mixed float", function (a)
  local t = 0
  for i = 1, 5000 do t = t + a * i end
  return t
end, 2.5)

same("div", function ()
  local d = 0.0
  for i = 1, 5000 do d = d + i / 7.0 end
  return d
end)

same("big loadk", function ()
  local s = 0
  for i = 1, 5000 do
    local x = 1099511627776
    s = s + x
  end
  return s
end)

same("big mulk", function ()
  local p = 1
  for i = 1, 100 do p = p * 1099511627776 end
  return p
end)

same("float loadk", function ()
  local t = 0.0
  for i = 1, 5000 do
    local c = 3.5
    t = t + c * i
  end
  return t
end)

same("float * int const", function ()
  local x = 0.5
  for i = 1, 1000 do x = x * 3 end
  return x
end)

same("float + int const", function ()
  local x = 0.5
  for i = 1, 1000 do x = x + 2 end
  return x
end)

same("float sub in place", function ()
  local x = 1000.0
  for i = 1, 5000 do x = x - i * 0.5 end
  return x
end)

same("float div in place", function ()
  local x = 1000.0
  for i = 1, 5000 do x = x / 2 end
  return x
end)

same("a==c mixed", function ()
  local x = 0
  for i = 1, 5000 do x = i + x end
  return x
end)

same("loadf body", function ()
  local s = 0.0
  for i = 1, 5000 do
    local z = 0.0
    s = s + z + i
  end
  return s
end)

same("neg zero", function ()
  local z = 0.0
  for i = 1, 5000 do z = -0.0 * i end
  return z
end)

-- a break must keep the loop on the interpreter
same("if lt", function ()
  local s = 0
  for i = 1, 5000 do if i < 100 then s = s + i end end
  return s
end)

same("if else", function ()
  local s = 0
  for i = 1, 5000 do
    if i <= 1000 then s = s + 1 else s = s + 2 end
  end
  return s
end)

same("if eq", function ()
  local s = 0
  for i = 1, 5000 do if i == 4321 then s = s + 7 end end
  return s
end)

same("nested if", function ()
  local s = 0
  for i = 1, 5000 do
    if i < 1000 then
      if i < 10 then s = s + 1 end
    end
  end
  return s
end)

same("if gt", function ()
  local s = 0
  for i = 1, 5000 do if i > 4000 then s = s + i end end
  return s
end)

same("if ne", function ()
  local s = 0
  for i = 1, 5000 do if i ~= 5000 then s = s + 1 end end
  return s
end)

same("break", function ()
  local s = 0
  for i = 1, 5000 do
    if i == 100 then break end
    s = s + i
  end
  return s
end)

-- calling the same Proto with a different type after compilation
local function typed (a)
  local t = 0
  for i = 1, 5000 do t = t + a * i end
  return t
end
jit.off()
local want_i, want_f = typed(2), typed(2.5)
jit.flush()
jit.on()
H.eq(typed(2), want_i)
H.eq(typed(2.5), want_f)
H.eq(typed(2), want_i)

-- jit.off() after compilation must fall back to the interpreter
local function sum (n)
  local s = 0
  for i = 1, n do s = s + i end
  return s
end
jit.flush(); jit.on()
H.eq(sum(5000), 12502500)
jit.off()
H.eq(sum(5000), 12502500)
H.check(jit.status() == false)
jit.on()
H.eq(sum(5000), 12502500)

print("jit/03_guards ok")
