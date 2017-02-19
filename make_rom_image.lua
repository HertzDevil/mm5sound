#!/usr/bin/env lua

local f = io.open(arg[1], "rb")
local g = io.open(arg[2], "w")

f:seek("set", 0x30010)
g:write [[
#pragma once

#include <cstdint>
#include <array>

static constexpr const std::array<uint8_t, 0x6000> MM5ROM {
]]
for i = 1, 0x6000 do
  g:write(("%s0x%02X%s"):format(
    i % 0x10 == 1 and '\t' or ' ', f:read(1):byte(),
    i % 0x10 == 0 and ',\n' or ','))
end
g:write "};\n"

f:close()
g:close()
