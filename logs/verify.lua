#!/usr/bin/env lua

-- run this in the parent directory
local scan = function (track)
  print("Checking song " .. track .. "...")
  local orig = io.open(("logs/%03d.log"):format(track))
  local test = io.popen(("./mm5test %d %d"):format(track - 1, 10800))
  local f = orig:lines()
  local g = test:lines()
  local row = 1
  while true do
    local x, y = f(), g()
    if not x and not y then break end
    if (x == nil) ~= (y == nil) then
      print "File size mismatch"
      orig:close()
      test:close()
      return false
    end
    if x ~= y then
      print("Line mismatch at row " .. row)
      orig:close()
      test:close()
      return false
    end
    row = row + 1
  end
  orig:close()
  test:close()
  return true
end

local fail = {}
for i = 1, 76 do
  if not scan(i) then
    fail[i] = true
  end
end

if not next(fail) then
  print "Success."
  os.exit(0)
end
print "Some songs failed:"
for i = 1, 76 do
  if fail[i] then
    io.write(i .. '\t')
  end
end
io.write '\n'
os.exit(1)
