#include <windows.h>
#include <iostream>

// Original function bytes (saved for unhooking if needed)
BYTE originalBytes[5];

// Typedef for the original function
typedef void (*ReduceHealth_t)(int);
ReduceHealth_t oReduceHealth = nullptr;

// Hook function: Does nothing (prevents health reduction)
void hReduceHealth(int dmg) {
    std::cout << "God mode active! Health not reduced (ignored " << dmg << " damage)." << std::endl;
    // Optionally call original: oReduceHealth(dmg); but for god mode, skip it
}

// Install the hook
bool installHook() {
    HMODULE hMod = GetModuleHandle(NULL);  // Handle to the exe (game process)
    if (!hMod) return false;

    FARPROC pFunc = GetProcAddress(hMod, "reduceHealth");
    if (!pFunc) {
        std::cout << "Failed to find reduceHealth address!" << std::endl;
        return false;
    }

    // Get the address as pointer
    void* funcAddr = (void*)pFunc;

    // Make memory writable
    DWORD oldProtect;
    if (!VirtualProtect(funcAddr, sizeof(originalBytes), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        std::cout << "Failed to change memory protection!" << std::endl;
        return false;
    }

    // Save original bytes
    memcpy(originalBytes, funcAddr, sizeof(originalBytes));

    // Create JMP instruction (x64: E9 rel32)
    // Calculate relative offset: hReduceHealth address - (funcAddr + 5)
    BYTE jmpInstruction[5] = { 0xE9, 0x00, 0x00, 0x00, 0x00 };
    int32_t offset = (int32_t)((uintptr_t)hReduceHealth - (uintptr_t)funcAddr - 5);
    memcpy(&jmpInstruction[1], &offset, sizeof(offset));

    // Write the hook
    memcpy(funcAddr, jmpInstruction, sizeof(jmpInstruction));

    // Restore protection
    VirtualProtect(funcAddr, sizeof(originalBytes), oldProtect, &oldProtect);

    std::cout << "Hook installed! God mode enabled." << std::endl;
    return true;
}

// Exported function to call from Lua
extern "C" __declspec(dllexport) void enableGodMode() {
    installHook();
}

// DLL entry point (runs on injection, but we trigger hook via enableGodMode for control)
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        // Optional: AllocConsole() for output if needed
        break;
    case DLL_PROCESS_DETACH:
        // Optional: Restore original bytes here
        break;
    }
    return TRUE;
}