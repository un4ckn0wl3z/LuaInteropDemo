#define _CRT_SECURE_NO_WARNINGS
#define LUA_BUILD_AS_DLL

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <psapi.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

// Function to get module base address
HMODULE GetModuleBaseAddress(DWORD pid, const char* moduleName) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) return NULL;

    HMODULE hModules[1024];
    DWORD cbNeeded;

    if (EnumProcessModules(hProcess, hModules, sizeof(hModules), &cbNeeded)) {
        for (DWORD i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            char szModuleName[MAX_PATH];
            if (GetModuleFileNameExA(hProcess, hModules[i], szModuleName, sizeof(szModuleName))) {
                char* baseName = strrchr(szModuleName, '\\');
                if (baseName) baseName++;
                else baseName = szModuleName;

                if (_stricmp(baseName, moduleName) == 0) {
                    CloseHandle(hProcess);
                    return hModules[i];
                }
            }
        }
    }

    CloseHandle(hProcess);
    return NULL;
}

// Lua function: getAddress(moduleName)
static int lua_getAddress(lua_State* L) {
    const char* moduleName = luaL_checkstring(L, 1);
    DWORD pid = GetCurrentProcessId();

    HMODULE hModule = GetModuleBaseAddress(pid, moduleName);
    if (hModule) {
        lua_pushinteger(L, (lua_Integer)hModule);
    }
    else {
        lua_pushinteger(L, 0);
    }

    return 1;
}

// Lua function: executeCodeEx(callingConvention, paramStruct, address)
static int lua_executeCodeEx(lua_State* L) {
    int callingConvention = (int)luaL_checkinteger(L, 1);
    // paramStruct not implemented in this example
    lua_Integer address = luaL_checkinteger(L, 3);

    // Simple function call implementation
    typedef void(__stdcall* StdCallFunc)();
    typedef void(__cdecl* CDeclFunc)();

    if (callingConvention == 1) { // __stdcall
        StdCallFunc func = (StdCallFunc)address;
        func();
    }
    else { // __cdecl or other
        CDeclFunc func = (CDeclFunc)address;
        func();
    }

    lua_pushboolean(L, 1);
    return 1;
}

// Enhanced function to get any address (module!function format)
static int lua_getAddressEnhanced(lua_State* L) {
    const char* addressSpec = luaL_checkstring(L, 1);

    // Check if it's in format "module!function"
    char* exclamation = strchr(addressSpec, '!');
    if (exclamation) {
        // Extract module name
        size_t moduleNameLen = exclamation - addressSpec;
        char moduleName[MAX_PATH];
        strncpy(moduleName, addressSpec, moduleNameLen);
        moduleName[moduleNameLen] = '\0';

        // Extract function name
        const char* functionName = exclamation + 1;

        // Get module handle
        HMODULE hModule = GetModuleHandleA(moduleName);
        if (!hModule) {
            lua_pushinteger(L, 0);
            return 1;
        }

        // Get function address
        FARPROC funcAddr = GetProcAddress(hModule, functionName);
        if (!funcAddr) {
            lua_pushinteger(L, 0);
            return 1;
        }

        lua_pushinteger(L, (lua_Integer)funcAddr);
        return 1;
    }

    // If not in module!function format, try as module name
    DWORD pid = GetCurrentProcessId();
    HMODULE hModule = GetModuleBaseAddress(pid, addressSpec);
    if (hModule) {
        lua_pushinteger(L, (lua_Integer)hModule);
    }
    else {
        lua_pushinteger(L, 0);
    }

    return 1;
}

// Read memory from address
static int lua_readMemory(lua_State* L) {
    lua_Integer address = luaL_checkinteger(L, 1);
    int size = (int)luaL_checkinteger(L, 2);

    if (size <= 0 || size > 1024) {
        lua_pushnil(L);
        return 1;
    }

    void* buffer = malloc(size);
    if (!buffer) {
        lua_pushnil(L);
        return 1;
    }

    // Read process memory
    SIZE_T bytesRead;
    if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)address, buffer, size, &bytesRead)) {
        lua_pushlstring(L, (const char*)buffer, bytesRead);
    }
    else {
        lua_pushnil(L);
    }

    free(buffer);
    return 1;
}

// Write memory to address
static int lua_writeMemory(lua_State* L) {
    lua_Integer address = luaL_checkinteger(L, 1);
    size_t dataSize;
    const char* data = luaL_checklstring(L, 2, &dataSize);

    SIZE_T bytesWritten;
    if (WriteProcessMemory(GetCurrentProcess(), (LPVOID)address, data, dataSize, &bytesWritten)) {
        lua_pushinteger(L, bytesWritten);
    }
    else {
        lua_pushinteger(L, 0);
    }

    return 1;
}

// Register all Lua functions
static void RegisterLuaFunctions(lua_State* L) {
    lua_register(L, "getAddress", lua_getAddressEnhanced);
    lua_register(L, "executeCodeEx", lua_executeCodeEx);
    lua_register(L, "readMemory", lua_readMemory);
    lua_register(L, "writeMemory", lua_writeMemory);

    // Also register the original getAddress for backward compatibility
    lua_register(L, "getModuleBaseAddress", lua_getAddress);
}

// Function to execute Lua code
int ExecuteLuaScript(lua_State* L, const char* script) {
    RegisterLuaFunctions(L);

    int status = luaL_loadstring(L, script);
    if (status == LUA_OK) {
        status = lua_pcall(L, 0, LUA_MULTRET, 0);
    }
    return status;
}

// Export function for remote execution
__declspec(dllexport) void RunLuaScript(const char* script) {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    int result = ExecuteLuaScript(L, script);
    if (result != LUA_OK) {
        const char* error = lua_tostring(L, -1);
        // You might want to log this error
        printf("Error: %s", error);
    }

    lua_close(L);
}

// Export function with error reporting
__declspec(dllexport) int RunLuaScriptWithResult(const char* script, char* output, size_t output_size) {
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    RegisterLuaFunctions(L);

    int result = luaL_loadstring(L, script);
    if (result == LUA_OK) {
        result = lua_pcall(L, 0, LUA_MULTRET, 0);
    }

    if (result != LUA_OK && output && output_size > 0) {
        const char* error = lua_tostring(L, -1);
        if (error) {
            strncpy(output, error, output_size - 1);
            output[output_size - 1] = '\0';
        }
    }

    lua_close(L);
    return result;
}

// Read file contents into a buffer
char* ReadFileContents(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = (char*)malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, file_size, file);
    buffer[file_size] = '\0';
    fclose(file);

    return buffer;
}


// Export function for remote execution with file path
__declspec(dllexport) void RunLuaScriptFromFile(const char* filename) {
    char* script = ReadFileContents(filename);
    if (!script) {
        // Handle file read error
        printf("Read lua script failed.\n");
        return;
    }

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);

    int result = ExecuteLuaScript(L, script);
    if (result != LUA_OK) {
        const char* error = lua_tostring(L, -1);
        // Log error if needed
        printf("Error: %s", error);
    }

    lua_close(L);
    free(script);
}

// Standard DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        // Optional: Run script automatically on injection
        RunLuaScript("print('DLL injected successfully!')");
        RunLuaScriptFromFile("..\\godmod.lua");
            break;
    case DLL_PROCESS_DETACH:
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}