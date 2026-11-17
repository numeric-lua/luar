-- tests/helpers.lua -- tiny assertion helpers shared by the suites.

local H = {}

local function fmt (v)
  if type(v) == "string" then return string.format("%q", v) end
  return tostring(v)
end

function H.check (cond, msg)
  if not cond then error(msg or "assertion failed", 2) end
end

function H.eq (got, want, msg)
  if got ~= want then
    error((msg or "values differ") .. ": got " .. fmt(got) ..
          ", want " .. fmt(want), 2)
  end
end

function H.near (got, want, eps, msg)
  eps = eps or 1e-9
  if type(got) ~= "number" or math.abs(got - want) > eps then
    error((msg or "numbers differ") .. ": got " .. tostring(got) ..
          ", want " .. tostring(want), 2)
  end
end

function H.raises (f, pattern, msg)
  local ok, err = pcall(f)
  if ok then error((msg or "expected an error") .. ": call succeeded", 2) end
  if pattern and not tostring(err):find(pattern) then
    error((msg or "wrong error") .. ": got " .. tostring(err), 2)
  end
  return err
end

return H
