#!/usr/bin/env lua

-- run this in the parent directory
local enable = false
for str in io.lines "nsf_write.log" do
  local track = str:match "(%d%d%d)/%d%d%d"
  if track then
    local fname = "logs/" .. track .. ".log"
    io.output(io.open(fname, "w"))
    io.stdout:write(("Writing %s...\n"):format(fname))
    enable = true
  elseif str:find("10800", 6, true) then
    enable = false
  elseif enable then
    io.write(str:gsub("\r", ""), '\n')
  end
end
