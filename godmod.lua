-- Get the base address of godmod.dll (already injected)
local dllName = "godmod.dll"
local dllBase = getAddress(dllName)
if dllBase == 0 then
    print("Error: Could not find godmod.dll in the process")
    return
end
print("godmod.dll found at address: 0x" .. string.format("%X", dllBase))

-- Get the address of enableGodMode (exported function)
local funcName = "godmod" .. "!enableGodMode"
local funcAddr = getAddress(funcName)
if funcAddr == 0 then
    print("Error: Could not find enableGodMode in godmod.dll")
    return
end
print("enableGodMode found at address: 0x" .. string.format("%X", funcAddr))

-- Call enableGodMode with __stdcall
executeCodeEx(1, nil, funcAddr)
print("Lua: God mode hook triggered!")
