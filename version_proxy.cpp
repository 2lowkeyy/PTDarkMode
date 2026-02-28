#include <windows.h>
#include <string>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

static HMODULE g_hReal       = nullptr;
static HMODULE g_hPTDarkMenu = nullptr;

static void LoadRealDll()
{
    wchar_t path[MAX_PATH];
    GetSystemDirectoryW(path, MAX_PATH);
    wcscat_s(path, L"\\version.dll");
    g_hReal = LoadLibraryW(path);
}

static FARPROC Real(const char* name)
{
    return g_hReal ? GetProcAddress(g_hReal, name) : nullptr;
}

typedef BOOL  (WINAPI* fn_GetFileVersionInfoA)      (LPCSTR,DWORD,DWORD,LPVOID);
typedef BOOL  (WINAPI* fn_GetFileVersionInfoW)      (LPCWSTR,DWORD,DWORD,LPVOID);
typedef DWORD (WINAPI* fn_GetFileVersionInfoSizeA)  (LPCSTR,LPDWORD);
typedef DWORD (WINAPI* fn_GetFileVersionInfoSizeW)  (LPCWSTR,LPDWORD);
typedef BOOL  (WINAPI* fn_GetFileVersionInfoExA)    (DWORD,LPCSTR,DWORD,DWORD,LPVOID);
typedef BOOL  (WINAPI* fn_GetFileVersionInfoExW)    (DWORD,LPCWSTR,DWORD,DWORD,LPVOID);
typedef DWORD (WINAPI* fn_GetFileVersionInfoSizeExA)(DWORD,LPCSTR,LPDWORD);
typedef DWORD (WINAPI* fn_GetFileVersionInfoSizeExW)(DWORD,LPCWSTR,LPDWORD);
typedef DWORD (WINAPI* fn_VerFindFileA)             (DWORD,LPSTR,LPSTR,LPSTR,LPSTR,PUINT,LPSTR,PUINT);
typedef DWORD (WINAPI* fn_VerFindFileW)             (DWORD,LPWSTR,LPWSTR,LPWSTR,LPWSTR,PUINT,LPWSTR,PUINT);
typedef DWORD (WINAPI* fn_VerInstallFileA)          (DWORD,LPSTR,LPSTR,LPSTR,LPSTR,LPSTR,LPSTR,PUINT);
typedef DWORD (WINAPI* fn_VerInstallFileW)          (DWORD,LPWSTR,LPWSTR,LPWSTR,LPWSTR,LPWSTR,LPWSTR,PUINT);
typedef DWORD (WINAPI* fn_VerLanguageNameA)         (DWORD,LPSTR,DWORD);
typedef DWORD (WINAPI* fn_VerLanguageNameW)         (DWORD,LPWSTR,DWORD);
typedef BOOL  (WINAPI* fn_VerQueryValueA)           (LPCVOID,LPCSTR,LPVOID*,PUINT);
typedef BOOL  (WINAPI* fn_VerQueryValueW)           (LPCVOID,LPCWSTR,LPVOID*,PUINT);
typedef BOOL  (WINAPI* fn_GetFileVersionInfoByHandle)(DWORD,HANDLE,DWORD,LPVOID);

static fn_GetFileVersionInfoA       p_GetFileVersionInfoA       = nullptr;
static fn_GetFileVersionInfoW       p_GetFileVersionInfoW       = nullptr;
static fn_GetFileVersionInfoSizeA   p_GetFileVersionInfoSizeA   = nullptr;
static fn_GetFileVersionInfoSizeW   p_GetFileVersionInfoSizeW   = nullptr;
static fn_GetFileVersionInfoExA     p_GetFileVersionInfoExA     = nullptr;
static fn_GetFileVersionInfoExW     p_GetFileVersionInfoExW     = nullptr;
static fn_GetFileVersionInfoSizeExA p_GetFileVersionInfoSizeExA = nullptr;
static fn_GetFileVersionInfoSizeExW p_GetFileVersionInfoSizeExW = nullptr;
static fn_VerFindFileA              p_VerFindFileA              = nullptr;
static fn_VerFindFileW              p_VerFindFileW              = nullptr;
static fn_VerInstallFileA           p_VerInstallFileA           = nullptr;
static fn_VerInstallFileW           p_VerInstallFileW           = nullptr;
static fn_VerLanguageNameA          p_VerLanguageNameA          = nullptr;
static fn_VerLanguageNameW          p_VerLanguageNameW          = nullptr;
static fn_VerQueryValueA            p_VerQueryValueA            = nullptr;
static fn_VerQueryValueW            p_VerQueryValueW            = nullptr;
static fn_GetFileVersionInfoByHandle p_GetFileVersionInfoByHandle = nullptr;

static void InitPointers()
{
    p_GetFileVersionInfoA       = (fn_GetFileVersionInfoA)      Real("GetFileVersionInfoA");
    p_GetFileVersionInfoW       = (fn_GetFileVersionInfoW)      Real("GetFileVersionInfoW");
    p_GetFileVersionInfoSizeA   = (fn_GetFileVersionInfoSizeA)  Real("GetFileVersionInfoSizeA");
    p_GetFileVersionInfoSizeW   = (fn_GetFileVersionInfoSizeW)  Real("GetFileVersionInfoSizeW");
    p_GetFileVersionInfoExA     = (fn_GetFileVersionInfoExA)    Real("GetFileVersionInfoExA");
    p_GetFileVersionInfoExW     = (fn_GetFileVersionInfoExW)    Real("GetFileVersionInfoExW");
    p_GetFileVersionInfoSizeExA = (fn_GetFileVersionInfoSizeExA)Real("GetFileVersionInfoSizeExA");
    p_GetFileVersionInfoSizeExW = (fn_GetFileVersionInfoSizeExW)Real("GetFileVersionInfoSizeExW");
    p_VerFindFileA              = (fn_VerFindFileA)             Real("VerFindFileA");
    p_VerFindFileW              = (fn_VerFindFileW)             Real("VerFindFileW");
    p_VerInstallFileA           = (fn_VerInstallFileA)          Real("VerInstallFileA");
    p_VerInstallFileW           = (fn_VerInstallFileW)          Real("VerInstallFileW");
    p_VerLanguageNameA          = (fn_VerLanguageNameA)         Real("VerLanguageNameA");
    p_VerLanguageNameW          = (fn_VerLanguageNameW)         Real("VerLanguageNameW");
    p_VerQueryValueA            = (fn_VerQueryValueA)           Real("VerQueryValueA");
    p_VerQueryValueW            = (fn_VerQueryValueW)           Real("VerQueryValueW");
    p_GetFileVersionInfoByHandle= (fn_GetFileVersionInfoByHandle)Real("GetFileVersionInfoByHandle");
}

#pragma comment(linker, "/export:GetFileVersionInfoA=_proxy_GetFileVersionInfoA")
#pragma comment(linker, "/export:GetFileVersionInfoW=_proxy_GetFileVersionInfoW")
#pragma comment(linker, "/export:GetFileVersionInfoSizeA=_proxy_GetFileVersionInfoSizeA")
#pragma comment(linker, "/export:GetFileVersionInfoSizeW=_proxy_GetFileVersionInfoSizeW")
#pragma comment(linker, "/export:GetFileVersionInfoExA=_proxy_GetFileVersionInfoExA")
#pragma comment(linker, "/export:GetFileVersionInfoExW=_proxy_GetFileVersionInfoExW")
#pragma comment(linker, "/export:GetFileVersionInfoSizeExA=_proxy_GetFileVersionInfoSizeExA")
#pragma comment(linker, "/export:GetFileVersionInfoSizeExW=_proxy_GetFileVersionInfoSizeExW")
#pragma comment(linker, "/export:VerFindFileA=_proxy_VerFindFileA")
#pragma comment(linker, "/export:VerFindFileW=_proxy_VerFindFileW")
#pragma comment(linker, "/export:VerInstallFileA=_proxy_VerInstallFileA")
#pragma comment(linker, "/export:VerInstallFileW=_proxy_VerInstallFileW")
#pragma comment(linker, "/export:VerLanguageNameA=_proxy_VerLanguageNameA")
#pragma comment(linker, "/export:VerLanguageNameW=_proxy_VerLanguageNameW")
#pragma comment(linker, "/export:VerQueryValueA=_proxy_VerQueryValueA")
#pragma comment(linker, "/export:VerQueryValueW=_proxy_VerQueryValueW")
#pragma comment(linker, "/export:GetFileVersionInfoByHandle=_proxy_GetFileVersionInfoByHandle")

extern "C" {

BOOL  WINAPI _proxy_GetFileVersionInfoA(LPCSTR a,DWORD b,DWORD c,LPVOID d)              { return p_GetFileVersionInfoA       ? p_GetFileVersionInfoA(a,b,c,d)         : FALSE; }
BOOL  WINAPI _proxy_GetFileVersionInfoW(LPCWSTR a,DWORD b,DWORD c,LPVOID d)             { return p_GetFileVersionInfoW       ? p_GetFileVersionInfoW(a,b,c,d)         : FALSE; }
DWORD WINAPI _proxy_GetFileVersionInfoSizeA(LPCSTR a,LPDWORD b)                         { return p_GetFileVersionInfoSizeA   ? p_GetFileVersionInfoSizeA(a,b)         : 0;     }
DWORD WINAPI _proxy_GetFileVersionInfoSizeW(LPCWSTR a,LPDWORD b)                        { return p_GetFileVersionInfoSizeW   ? p_GetFileVersionInfoSizeW(a,b)         : 0;     }
BOOL  WINAPI _proxy_GetFileVersionInfoExA(DWORD a,LPCSTR b,DWORD c,DWORD d,LPVOID e)   { return p_GetFileVersionInfoExA     ? p_GetFileVersionInfoExA(a,b,c,d,e)     : FALSE; }
BOOL  WINAPI _proxy_GetFileVersionInfoExW(DWORD a,LPCWSTR b,DWORD c,DWORD d,LPVOID e)  { return p_GetFileVersionInfoExW     ? p_GetFileVersionInfoExW(a,b,c,d,e)     : FALSE; }
DWORD WINAPI _proxy_GetFileVersionInfoSizeExA(DWORD a,LPCSTR b,LPDWORD c)               { return p_GetFileVersionInfoSizeExA ? p_GetFileVersionInfoSizeExA(a,b,c)     : 0;     }
DWORD WINAPI _proxy_GetFileVersionInfoSizeExW(DWORD a,LPCWSTR b,LPDWORD c)              { return p_GetFileVersionInfoSizeExW ? p_GetFileVersionInfoSizeExW(a,b,c)     : 0;     }
DWORD WINAPI _proxy_VerFindFileA(DWORD a,LPSTR b,LPSTR c,LPSTR d,LPSTR e,PUINT f,LPSTR g,PUINT h)          { return p_VerFindFileA   ? p_VerFindFileA(a,b,c,d,e,f,g,h)   : 0;     }
DWORD WINAPI _proxy_VerFindFileW(DWORD a,LPWSTR b,LPWSTR c,LPWSTR d,LPWSTR e,PUINT f,LPWSTR g,PUINT h)     { return p_VerFindFileW   ? p_VerFindFileW(a,b,c,d,e,f,g,h)   : 0;     }
DWORD WINAPI _proxy_VerInstallFileA(DWORD a,LPSTR b,LPSTR c,LPSTR d,LPSTR e,LPSTR f,LPSTR g,PUINT h)       { return p_VerInstallFileA? p_VerInstallFileA(a,b,c,d,e,f,g,h): 0;     }
DWORD WINAPI _proxy_VerInstallFileW(DWORD a,LPWSTR b,LPWSTR c,LPWSTR d,LPWSTR e,LPWSTR f,LPWSTR g,PUINT h) { return p_VerInstallFileW? p_VerInstallFileW(a,b,c,d,e,f,g,h): 0;     }
DWORD WINAPI _proxy_VerLanguageNameA(DWORD a,LPSTR b,DWORD c)                           { return p_VerLanguageNameA ? p_VerLanguageNameA(a,b,c) : 0;     }
DWORD WINAPI _proxy_VerLanguageNameW(DWORD a,LPWSTR b,DWORD c)                          { return p_VerLanguageNameW ? p_VerLanguageNameW(a,b,c) : 0;     }
BOOL  WINAPI _proxy_VerQueryValueA(LPCVOID a,LPCSTR b,LPVOID* c,PUINT d)               { return p_VerQueryValueA   ? p_VerQueryValueA(a,b,c,d) : FALSE; }
BOOL  WINAPI _proxy_VerQueryValueW(LPCVOID a,LPCWSTR b,LPVOID* c,PUINT d)              { return p_VerQueryValueW   ? p_VerQueryValueW(a,b,c,d) : FALSE; }
BOOL  WINAPI _proxy_GetFileVersionInfoByHandle(DWORD a,HANDLE b,DWORD c,LPVOID d)       { return p_GetFileVersionInfoByHandle ? p_GetFileVersionInfoByHandle(a,b,c,d) : FALSE; }

}

static void LoadPTDarkMenu()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring path(exePath);
    size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
        path = path.substr(0, slash + 1);
    path += L"PTDarkMenu.dll";
    g_hPTDarkMenu = LoadLibraryW(path.c_str());
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        LoadRealDll();
        InitPointers();
        CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            Sleep(200);
            LoadPTDarkMenu();
            return 0;
        }, nullptr, 0, nullptr);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        if (g_hPTDarkMenu) FreeLibrary(g_hPTDarkMenu);
        if (g_hReal)       FreeLibrary(g_hReal);
    }
    return TRUE;
}
