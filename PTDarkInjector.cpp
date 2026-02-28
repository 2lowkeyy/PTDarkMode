#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <iostream>

static HMODULE GetRemoteModuleHandle(DWORD pid, const std::wstring& dllName)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return nullptr;

    MODULEENTRY32W me = {};
    me.dwSize = sizeof(me);

    HMODULE result = nullptr;
    if (Module32FirstW(snap, &me)) {
        do {
            if (_wcsicmp(me.szModule, dllName.c_str()) == 0) {
                result = me.hModule;
                break;
            }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return result;
}

static bool Inject(DWORD pid, const std::wstring& dllPath)
{
    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
        FALSE, pid);

    if (!hProcess) {
        std::wcerr << L"[Injector] OpenProcess failed: " << GetLastError() << L"\n";
        return false;
    }

    size_t pathSize = (dllPath.size() + 1) * sizeof(wchar_t);
    LPVOID pRemotePath = VirtualAllocEx(hProcess, nullptr, pathSize,
                                         MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pRemotePath) {
        std::wcerr << L"[Injector] VirtualAllocEx failed: " << GetLastError() << L"\n";
        CloseHandle(hProcess);
        return false;
    }

    if (!WriteProcessMemory(hProcess, pRemotePath, dllPath.c_str(), pathSize, nullptr)) {
        std::wcerr << L"[Injector] WriteProcessMemory failed: " << GetLastError() << L"\n";
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }


    HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!hKernel32) {
        std::wcerr << L"[Injector] GetModuleHandleW(\"kernel32.dll\") failed: " << GetLastError() << L"\n";
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }
    LPVOID pLoadLibrary = (LPVOID)GetProcAddress(hKernel32, "LoadLibraryW");

    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
        (LPTHREAD_START_ROUTINE)pLoadLibrary, pRemotePath, 0, nullptr);

    if (!hThread) {
        std::wcerr << L"[Injector] CreateRemoteThread failed: " << GetLastError() << L"\n";
        VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, 5000);

    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);

    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    if (!exitCode) {
        std::wcerr << L"[Injector] LoadLibraryW returned NULL in target process\n";
        return false;
    }

    std::wcout << L"[Injector] Injection successful. Remote module base: 0x"
               << std::hex << exitCode << L"\n";
    return true;
}

static bool Eject(DWORD pid, const std::wstring& dllPath)
{
    std::wstring dllName = dllPath;
    size_t slash = dllName.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
        dllName = dllName.substr(slash + 1);

    HMODULE hRemote = GetRemoteModuleHandle(pid, dllName);
    if (!hRemote) {
        std::wcerr << L"[Injector] DLL not found in target process\n";
        return false;
    }

    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION,
        FALSE, pid);
    if (!hProcess) {
        std::wcerr << L"[Injector] OpenProcess failed: " << GetLastError() << L"\n";
        return false;
    }

    HMODULE hLocalDll = LoadLibraryW(dllPath.c_str());
    if (hLocalDll) {
        FARPROC pRemoveAll = GetProcAddress(hLocalDll, "RemoveAllDarkMenus");
        if (pRemoveAll) {
            SIZE_T offset = (SIZE_T)pRemoveAll - (SIZE_T)hLocalDll;
            LPVOID pRemoteFunc = (LPVOID)((SIZE_T)hRemote + offset);

            HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
                (LPTHREAD_START_ROUTINE)pRemoteFunc, nullptr, 0, nullptr);
            if (hThread) {
                WaitForSingleObject(hThread, 3000);
                CloseHandle(hThread);
            }
        }
        FreeLibrary(hLocalDll);
    }

    LPVOID pFreeLibrary = (LPVOID)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "FreeLibrary");
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0,
        (LPTHREAD_START_ROUTINE)pFreeLibrary, hRemote, 0, nullptr);

    if (!hThread) {
        std::wcerr << L"[Injector] FreeLibrary thread failed: " << GetLastError() << L"\n";
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, 5000);
    CloseHandle(hThread);
    CloseHandle(hProcess);

    std::wcout << L"[Injector] Ejection successful\n";
    return true;
}

int wmain(int argc, wchar_t* argv[])
{
    if (argc < 4) {
        std::wcout << L"Usage:\n"
                   << L"  PTDarkInjector.exe inject <pid> <full_dll_path>\n"
                   << L"  PTDarkInjector.exe eject  <pid> <full_dll_path>\n";
        return 1;
    }

    std::wstring action  = argv[1];
    DWORD        pid     = (DWORD)_wtoi(argv[2]);
    std::wstring dllPath = argv[3];

    if (action == L"inject")
        return Inject(pid, dllPath) ? 0 : 1;
    else if (action == L"eject")
        return Eject(pid, dllPath) ? 0 : 1;
    else {
        std::wcerr << L"Unknown action: " << action << L"\n";
        return 1;
    }
}
