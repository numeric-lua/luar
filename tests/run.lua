-- tests/run.lua -- Luar test runner.
--
-- Usage: luar tests/run.lua [suite ...]
--   suite is a directory name under tests/ (default: all except bench).
--
-- Each test file is a plain Lua chunk.  A test fails if it raises an
-- error.  Files may use tests/helpers.lua for assertions.

local sep = package.config:sub(1, 1)

local function listdir (path)
  local files = {}
  local p = io.popen('ls -1 "' .. path .. '" 2>/dev/null')
  if not p then return files end
  for name in p:lines() do
    if name:match("%.lua$") then files[#files + 1] = name end
  end
  p:close()
  table.sort(files)
  return files
end


local suites = {...}
if #suites == 0 then
  suites = {"semantics", "compat", "jit", "ffi", "consumer"}
end

local total, failed = 0, 0
local failures = {}

for _, suite in ipairs(suites) do
  local dir = "tests" .. sep .. suite
  local files = listdir(dir)
  if #files == 0 then
    io.write(("-- %s: (no tests)\n"):format(suite))
  end
  for _, file in ipairs(files) do
    total = total + 1
    local path = dir .. sep .. file
    local chunk, err = loadfile(path)
    if not chunk then
      failed = failed + 1
      failures[#failures + 1] = path .. ": load error: " .. tostring(err)
    else
      local ok, rerr = pcall(chunk)
      if not ok then
        failed = failed + 1
        failures[#failures + 1] = path .. ": " .. tostring(rerr)
      end
    end
  end
end

io.write("\n")
for _, f in ipairs(failures) do
  io.write("FAIL ", f, "\n")
end
io.write(("%d test file(s), %d failure(s)\n"):format(total, failed))
os.exit(failed == 0 and 0 or 1)
