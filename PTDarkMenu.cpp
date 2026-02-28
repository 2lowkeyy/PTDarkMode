#include <windows.h>
#include <dwmapi.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#define WM_PTDARK_FS      (WM_USER+100)
#define WM_PTDARK_REPAINT (WM_USER+101)

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

static const COLORREF CLR_MENU_BG       = RGB(0x1A, 0x1A, 0x1A);
static const COLORREF CLR_MENU_TEXT     = RGB(0xFF, 0xFF, 0xFF);
static const COLORREF CLR_ITEM_HOVER_BG = RGB(0x3D, 0x3D, 0x3D);
static const COLORREF CLR_ITEM_HOVER_TX = RGB(0xFF, 0xFF, 0xFF);
static const COLORREF CLR_DISABLED_TX   = RGB(0x80, 0x80, 0x80);
static const COLORREF CLR_CAPTION_BG    = RGB(0x1A, 0x1A, 0x1A);
static const COLORREF CLR_CAPTION_TX    = RGB(0xFF, 0xFF, 0xFF);

enum FullscreenMode {
    FS_NONE       = 0,
    FS_WINDOWED   = 1,  
    FS_NO_CAPTION = 2,  
    FS_NO_MENU    = 3,  
    FS_FULL       = 4,  
};


static HINSTANCE     g_hModule     = nullptr;
static HWND          g_hwndMain    = nullptr;
static HWINEVENTHOOK g_hEvHook     = nullptr;
static HHOOK         g_hKbHook     = nullptr;
static HANDLE        g_hMonThread  = nullptr;  
static HANDLE        g_hHookThread = nullptr; 
static bool          g_running     = false;
static DWORD         g_hookThreadId = 0;

static FullscreenMode g_fsMode     = FS_NONE;
static HMENU          g_hMenuSaved = nullptr;
static RECT           g_rcOrig     = {};      
static bool           g_fsApplied  = false;   

static std::unordered_map<HWND, WNDPROC> g_subclassed;
static std::unordered_set<HWND>          g_darkened;

static HBRUSH g_brBg        = nullptr;
static HBRUSH g_brHover     = nullptr;
static HFONT  g_hMenuFont   = nullptr;
static bool   g_dragging    = false;   
static int    g_lastHotItem = -2;      

struct MenuCache {
    HBITMAP hBmp  = nullptr;
    HDC     hDC   = nullptr;
    int     w     = 0, h = 0;
    int     hot   = -1;
    bool    valid = false;
    void Destroy()    { if(hDC){DeleteDC(hDC);hDC=nullptr;} if(hBmp){DeleteObject(hBmp);hBmp=nullptr;} valid=false;w=0;h=0;hot=-1; }
    void Invalidate() { valid=false; }
} g_mc;

static void InitGDI() {
    if(!g_brBg)     g_brBg     = CreateSolidBrush(CLR_MENU_BG);
    if(!g_brHover)  g_brHover  = CreateSolidBrush(CLR_ITEM_HOVER_BG);
    if(!g_hMenuFont){
        NONCLIENTMETRICSW ncm={sizeof(ncm)};
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof(ncm),&ncm,0);
        g_hMenuFont=CreateFontIndirectW(&ncm.lfMenuFont);
    }
}
static void DestroyGDI() {
    g_mc.Destroy();
    if(g_brBg)    {DeleteObject(g_brBg);    g_brBg=nullptr;}
    if(g_brHover) {DeleteObject(g_brHover); g_brHover=nullptr;}
    if(g_hMenuFont){DeleteObject(g_hMenuFont);g_hMenuFont=nullptr;}
}

static void DWMSet(HWND h, DWORD attr, DWORD val)
{
    DwmSetWindowAttribute(h, attr, &val, sizeof(val));
}

static void ApplyDarkCaption(HWND h)
{
    DWMSet(h, 20, 1);
    DWMSet(h, 35, (DWORD)CLR_CAPTION_BG);
    DWMSet(h, 36, (DWORD)CLR_CAPTION_TX);
}

static void ResetCaption(HWND h)
{
    DWMSet(h, 20, 0);
    DWMSet(h, 35, 0xFFFFFFFF);
    DWMSet(h, 36, 0xFFFFFFFF);
}

static void FrameRedraw(HWND h)
{
    SetWindowPos(h, nullptr, 0,0,0,0,
        SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED|SWP_NOACTIVATE);
}

static bool IsOurPID(HWND h)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    return pid == GetCurrentProcessId();
}

static bool ClassIs(HWND h, const wchar_t* name)
{
    wchar_t cls[64]={};
    GetClassNameW(h, cls, 63);
    return _wcsicmp(cls, name) == 0;
}

static bool IsPlugin(HWND h)
{
    return ClassIs(h, L"DigiFloaterClass") || ClassIs(h, L"DigiWndClass");
}

static HWND FindMainWindow()
{
    struct Ctx { DWORD pid; HWND found; };
    Ctx ctx = { GetCurrentProcessId(), nullptr };
    EnumWindows([](HWND h, LPARAM lp)->BOOL{
        auto* c=(Ctx*)lp;
        DWORD pid=0; GetWindowThreadProcessId(h,&pid);
        if(pid!=c->pid) return TRUE;
        wchar_t cls[64]={}; GetClassNameW(h,cls,63);
        if(_wcsicmp(cls,L"DigiAppWndClass")==0){c->found=h;return FALSE;}
        return TRUE;
    },(LPARAM)&ctx);
    return ctx.found;
}

static HWND FindMDIChild(HWND parent, const wchar_t* prefix)
{
    struct Ctx { const wchar_t* prefix; HWND found; };
    Ctx ctx = { prefix, nullptr };
    EnumChildWindows(parent,[](HWND h,LPARAM lp)->BOOL{
        auto* c=(Ctx*)lp;
        wchar_t t[256]={}; GetWindowTextW(h,t,255);
        if(wcsncmp(t,c->prefix,wcslen(c->prefix))==0){c->found=h;return FALSE;}
        return TRUE;
    },(LPARAM)&ctx);
    return ctx.found;
}

static RECT GetWorkArea()
{
    HMONITOR hm = MonitorFromWindow(g_hwndMain, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi={sizeof(mi)};
    GetMonitorInfo(hm,&mi);
    return mi.rcWork;
}

static void RemoveBorders(HWND h)
{
    LONG s = GetWindowLong(h, GWL_STYLE);
    s &= ~(WS_CAPTION|WS_SIZEBOX|WS_THICKFRAME);
    SetWindowLong(h, GWL_STYLE, s);
    FrameRedraw(h);
}

static void RestoreBorders(HWND h)
{
    LONG s = GetWindowLong(h, GWL_STYLE);
    s |= (WS_CAPTION|WS_SIZEBOX|WS_THICKFRAME);
    SetWindowLong(h, GWL_STYLE, s);
    FrameRedraw(h);
}

static void FillMDI(HWND parent, HWND child)
{
    if(!child) return;
    RECT rc; GetClientRect(parent,&rc);
    SetWindowPos(child,nullptr,rc.left,rc.top,
        rc.right-rc.left,rc.bottom-rc.top,
        SWP_NOZORDER|SWP_NOACTIVATE);
}

static void ApplyFS()
{
    if(!g_hwndMain || !IsWindow(g_hwndMain)) return;

    HWND hEdit = FindMDIChild(g_hwndMain, L"Edit:");
    HWND hMix  = FindMDIChild(g_hwndMain, L"Mix:");
    RECT rcWork = GetWorkArea();
    LONG style  = GetWindowLong(g_hwndMain, GWL_STYLE);

    switch(g_fsMode)
    {
    case FS_NONE:
        style |= (WS_CAPTION|WS_SIZEBOX|WS_THICKFRAME);
        SetWindowLong(g_hwndMain, GWL_STYLE, style);
        if(g_hMenuSaved){ SetMenu(g_hwndMain,g_hMenuSaved); g_hMenuSaved=nullptr; }

        if(g_rcOrig.right > g_rcOrig.left)
            SetWindowPos(g_hwndMain,nullptr,
                g_rcOrig.left, g_rcOrig.top,
                g_rcOrig.right-g_rcOrig.left,
                g_rcOrig.bottom-g_rcOrig.top,
                SWP_NOZORDER|SWP_FRAMECHANGED);

        if(hEdit) RestoreBorders(hEdit);
        if(hMix)  RestoreBorders(hMix);
        ApplyDarkCaption(g_hwndMain);
        break;

    case FS_WINDOWED:
        if(!g_rcOrig.right) GetWindowRect(g_hwndMain,&g_rcOrig);

        style |= (WS_CAPTION|WS_SIZEBOX|WS_THICKFRAME);
        SetWindowLong(g_hwndMain, GWL_STYLE, style);
        if(g_hMenuSaved){ SetMenu(g_hwndMain,g_hMenuSaved); g_hMenuSaved=nullptr; }

        SetWindowPos(g_hwndMain,HWND_TOP,
            rcWork.left,rcWork.top,
            rcWork.right-rcWork.left,rcWork.bottom-rcWork.top,
            SWP_FRAMECHANGED);

        if(hEdit){ RemoveBorders(hEdit); FillMDI(g_hwndMain,hEdit); }
        if(hMix) { RemoveBorders(hMix);  FillMDI(g_hwndMain,hMix);  }
        ApplyDarkCaption(g_hwndMain);
        break;

    case FS_NO_CAPTION:
        if(!g_rcOrig.right) GetWindowRect(g_hwndMain,&g_rcOrig);
        style &= ~(WS_CAPTION|WS_SIZEBOX|WS_THICKFRAME);
        SetWindowLong(g_hwndMain, GWL_STYLE, style);
        if(g_hMenuSaved){ SetMenu(g_hwndMain,g_hMenuSaved); g_hMenuSaved=nullptr; }
        SetWindowPos(g_hwndMain,HWND_TOP,
            rcWork.left,rcWork.top,
            rcWork.right-rcWork.left,rcWork.bottom-rcWork.top,
            SWP_FRAMECHANGED);
        if(hEdit){ RemoveBorders(hEdit); FillMDI(g_hwndMain,hEdit); }
        if(hMix) { RemoveBorders(hMix);  FillMDI(g_hwndMain,hMix);  }
        break;

    case FS_NO_MENU:
        if(!g_rcOrig.right) GetWindowRect(g_hwndMain,&g_rcOrig);
        if(!g_hMenuSaved){ g_hMenuSaved=GetMenu(g_hwndMain); SetMenu(g_hwndMain,nullptr); }
        style |= (WS_CAPTION|WS_SIZEBOX);
        SetWindowLong(g_hwndMain, GWL_STYLE, style);
        SetWindowPos(g_hwndMain,HWND_TOP,
            rcWork.left,rcWork.top,
            rcWork.right-rcWork.left,rcWork.bottom-rcWork.top,
            SWP_FRAMECHANGED);
        if(hEdit){ RemoveBorders(hEdit); FillMDI(g_hwndMain,hEdit); }
        if(hMix) { RemoveBorders(hMix);  FillMDI(g_hwndMain,hMix);  }
        ApplyDarkCaption(g_hwndMain);
        break;

    case FS_FULL:
        if(!g_rcOrig.right) GetWindowRect(g_hwndMain,&g_rcOrig);
        style &= ~(WS_CAPTION|WS_SIZEBOX|WS_THICKFRAME);
        SetWindowLong(g_hwndMain, GWL_STYLE, style);
        if(!g_hMenuSaved){ g_hMenuSaved=GetMenu(g_hwndMain); SetMenu(g_hwndMain,nullptr); }
        SetWindowPos(g_hwndMain,HWND_TOP,
            rcWork.left,rcWork.top,
            rcWork.right-rcWork.left,rcWork.bottom-rcWork.top,
            SWP_FRAMECHANGED);
        if(hEdit){ RemoveBorders(hEdit); FillMDI(g_hwndMain,hEdit); }
        if(hMix) { RemoveBorders(hMix);  FillMDI(g_hwndMain,hMix);  }
        break;
    }
}

static void ToggleFS(FullscreenMode mode)
{
    if(g_fsMode == mode){
        g_fsMode = FS_NONE;
    } else {
        if(g_fsMode == FS_NONE) GetWindowRect(g_hwndMain,&g_rcOrig);
        g_fsMode = mode;
    }
    ApplyFS();
}

static void BuildMenuCache(HWND h, int hotItem)
{
    HMENU hMenu=GetMenu(h); if(!hMenu) return;
    MENUBARINFO mbi={sizeof(mbi)};
    if(!GetMenuBarInfo(h,OBJID_MENU,0,&mbi)) return;
    RECT rcWin; GetWindowRect(h,&rcWin);
    RECT rcBar=mbi.rcBar;
    OffsetRect(&rcBar,-rcWin.left,-rcWin.top);
    int bw=rcBar.right-rcBar.left, bh=rcBar.bottom-rcBar.top;
    if(bw<=0||bh<=0) return;

    HDC hdcWin=GetWindowDC(h); if(!hdcWin) return;
    if(!g_mc.hDC||bw!=g_mc.w||bh!=g_mc.h){
        g_mc.Destroy();
        g_mc.hDC =CreateCompatibleDC(hdcWin);
        g_mc.hBmp=CreateCompatibleBitmap(hdcWin,bw,bh);
        SelectObject(g_mc.hDC,g_mc.hBmp);
        g_mc.w=bw; g_mc.h=bh;
    }
    ReleaseDC(h,hdcWin);

    HDC dc=g_mc.hDC;
    RECT fill={0,0,bw,bh}; FillRect(dc,&fill,g_brBg);
    HFONT hfOld=(HFONT)SelectObject(dc,g_hMenuFont);
    SetBkMode(dc,TRANSPARENT);

    int n=GetMenuItemCount(hMenu);
    for(int i=0;i<n;i++){
        RECT ri={}; if(!GetMenuItemRect(h,hMenu,i,&ri)) continue;
        OffsetRect(&ri,-rcWin.left-rcBar.left,-rcWin.top-rcBar.top);
        MENUITEMINFOW mii={sizeof(mii)};
        mii.fMask=MIIM_STATE|MIIM_STRING;
        wchar_t buf[256]={}; mii.dwTypeData=buf; mii.cch=255;
        GetMenuItemInfoW(hMenu,i,TRUE,&mii);
        bool hot=(i==hotItem);
        bool dis=(mii.fState&MFS_GRAYED)!=0;
        FillRect(dc,&ri,hot?g_brHover:g_brBg);
        SetTextColor(dc,dis?CLR_DISABLED_TX:hot?CLR_ITEM_HOVER_TX:CLR_MENU_TEXT);
        RECT rt=ri; rt.left+=8; rt.right-=8;
        DrawTextW(dc,buf,-1,&rt,DT_SINGLELINE|DT_VCENTER|DT_CENTER);
    }
    SelectObject(dc,hfOld);
    g_mc.hot=hotItem; g_mc.valid=true;
}

static void PaintMenu(HWND h, int hotItem)
{
    if(!g_mc.valid||g_mc.hot!=hotItem) BuildMenuCache(h,hotItem);
    if(!g_mc.valid) return;
    MENUBARINFO mbi={sizeof(mbi)};
    if(!GetMenuBarInfo(h,OBJID_MENU,0,&mbi)) return;
    RECT rcWin; GetWindowRect(h,&rcWin);
    RECT rcBar=mbi.rcBar;
    OffsetRect(&rcBar,-rcWin.left,-rcWin.top);
    HDC hdc=GetWindowDC(h); if(!hdc) return;
    BitBlt(hdc,rcBar.left,rcBar.top,g_mc.w,g_mc.h,g_mc.hDC,0,0,SRCCOPY);
    ReleaseDC(h,hdc);
}

static int GetHotItem(HWND h){
    HMENU hm=GetMenu(h); if(!hm) return -1;
    POINT pt; GetCursorPos(&pt);
    int n=GetMenuItemCount(hm);
    for(int i=0;i<n;i++){RECT r={};if(GetMenuItemRect(h,hm,i,&r)&&PtInRect(&r,pt))return i;}
    return -1;
}

static LRESULT CALLBACK SubclassProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    auto it=g_subclassed.find(h);
    WNDPROC orig=(it!=g_subclassed.end())?it->second:DefWindowProcW;

    if(msg==WM_PTDARK_FS){ ToggleFS((FullscreenMode)wp); return 0; }

    auto Repaint=[&]{ if(!g_dragging) PaintMenu(h, g_lastHotItem); };

    switch(msg){
    case WM_NCPAINT:
        CallWindowProcW(orig,h,msg,wp,lp);
        Repaint(); return 0;

    case WM_NCACTIVATE:{
        LRESULT r=CallWindowProcW(orig,h,msg,wp,lp);
        Repaint(); return r;}

    case WM_MENUSELECT:
        g_mc.Invalidate();
        { LRESULT r=CallWindowProcW(orig,h,msg,wp,lp); Repaint(); return r; }

    case WM_NCMOUSEMOVE:{
        if(g_dragging) return CallWindowProcW(orig,h,msg,wp,lp);
        int hot=GetHotItem(h);
        LRESULT r=CallWindowProcW(orig,h,msg,wp,lp);
        if(hot!=g_lastHotItem){ g_lastHotItem=hot; Repaint(); }
        return r;}

    case WM_NCLBUTTONDOWN:{
        LRESULT r=CallWindowProcW(orig,h,msg,wp,lp);
        PostMessageW(h,WM_PTDARK_REPAINT,0,0); return r;}

    case WM_PTDARK_REPAINT:
        Repaint(); return 0;

    case WM_ENTERSIZEMOVE:
        g_dragging=true;
        return CallWindowProcW(orig,h,msg,wp,lp);

    case WM_EXITSIZEMOVE:
        g_dragging=false;
        g_mc.Invalidate();
        { LRESULT r=CallWindowProcW(orig,h,msg,wp,lp); Repaint(); return r; }

    case WM_SIZE: case WM_MOVE:
        g_mc.Invalidate();
        return CallWindowProcW(orig,h,msg,wp,lp);

    case 0x0091: case 0x0092: 
        Repaint(); return 0;

    case WM_DESTROY:
        if(it!=g_subclassed.end()){
            SetWindowLongPtrW(h,GWLP_WNDPROC,(LONG_PTR)it->second);
            g_subclassed.erase(it);
        }
        return CallWindowProcW(orig,h,msg,wp,lp);

    default:
        return CallWindowProcW(orig,h,msg,wp,lp);
    }
}

static void SubclassMain(HWND h)
{
    if(g_subclassed.count(h)) return;
    WNDPROC orig=(WNDPROC)SetWindowLongPtrW(h,GWLP_WNDPROC,(LONG_PTR)SubclassProc);
    if(orig) g_subclassed[h]=orig;
}

static void DarkenPlugin(HWND h)
{
    if(g_darkened.count(h)) return;
    ApplyDarkCaption(h);
    FrameRedraw(h);
    g_darkened.insert(h);
}

static void CALLBACK WinEventProc(HWINEVENTHOOK,DWORD,HWND h,
    LONG idObj,LONG,DWORD,DWORD)
{
    if(!h||idObj!=OBJID_WINDOW) return;
    if(!IsOurPID(h)) return;
    if(IsPlugin(h)) DarkenPlugin(h);
}

static LRESULT CALLBACK KbProc(int code, WPARAM wp, LPARAM lp)
{
    if(code==HC_ACTION && (wp==WM_KEYDOWN||wp==WM_SYSKEYDOWN))
    {
        KBDLLHOOKSTRUCT* kb=(KBDLLHOOKSTRUCT*)lp;
        bool ctrl  = (GetAsyncKeyState(VK_CONTROL)&0x8000)!=0;
        bool shift = (GetAsyncKeyState(VK_SHIFT)  &0x8000)!=0;

        if(ctrl && shift && g_hwndMain && IsWindow(g_hwndMain))
        {
            if     (kb->vkCode==VK_F12) PostMessageW(g_hwndMain,WM_PTDARK_FS,FS_FULL,      0);
            else if(kb->vkCode==VK_F11) PostMessageW(g_hwndMain,WM_PTDARK_FS,FS_NO_CAPTION,0);
            else if(kb->vkCode==VK_F10) PostMessageW(g_hwndMain,WM_PTDARK_FS,FS_NO_MENU,   0);
            else if(kb->vkCode==VK_F9)  PostMessageW(g_hwndMain,WM_PTDARK_FS,FS_WINDOWED,  0);
        }
    }
    return CallNextHookEx(g_hKbHook,code,wp,lp);
}

static DWORD WINAPI HookThread(LPVOID)
{
    g_hKbHook = SetWindowsHookExW(WH_KEYBOARD_LL, KbProc, g_hModule, 0);

    MSG msg;
    while(g_running && GetMessageW(&msg,nullptr,0,0)>0){
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if(g_hKbHook){ UnhookWindowsHookEx(g_hKbHook); g_hKbHook=nullptr; }
    return 0;
}

static DWORD WINAPI MonitorThread(LPVOID)
{
    bool sessionApplied = false;

    while(g_running)
    {
        HWND hMain = FindMainWindow();
        if(hMain && hMain != g_hwndMain){
            g_hwndMain = hMain;
            InitGDI();
            ApplyDarkCaption(g_hwndMain);
            SubclassMain(g_hwndMain);
            FrameRedraw(g_hwndMain);
            sessionApplied = false; 
        }

        if(g_hwndMain && !sessionApplied){
            HWND hEdit = FindMDIChild(g_hwndMain, L"Edit:");
            HWND hMix  = FindMDIChild(g_hwndMain, L"Mix:");
            if(hEdit && hMix){

                g_fsMode = FS_WINDOWED;
                ApplyFS();
                sessionApplied = true;
            }
        }

        EnumWindows([](HWND h,LPARAM)->BOOL{
            if(IsOurPID(h)&&IsPlugin(h)) DarkenPlugin(h);
            return TRUE;
        },0);

        std::vector<HWND> dead;
        for(HWND hw:g_darkened) if(!IsWindow(hw)) dead.push_back(hw);
        for(HWND hw:dead) g_darkened.erase(hw);

        Sleep(500);
    }
    return 0;
}

static void Install()
{
    g_running = true;

    g_hHookThread = CreateThread(nullptr,0,HookThread,nullptr,0,&g_hookThreadId);

    g_hMonThread = CreateThread(nullptr,0,MonitorThread,nullptr,0,nullptr);

    g_hEvHook = SetWinEventHook(
        EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW,
        nullptr, WinEventProc,
        GetCurrentProcessId(), 0,
        WINEVENT_OUTOFCONTEXT);
}

static void Uninstall()
{
    g_running = false;

    if(g_hookThreadId) PostThreadMessageW(g_hookThreadId, WM_QUIT, 0, 0);
    if(g_hHookThread){ WaitForSingleObject(g_hHookThread,2000); CloseHandle(g_hHookThread); g_hHookThread=nullptr; }
    if(g_hMonThread) { WaitForSingleObject(g_hMonThread, 2000); CloseHandle(g_hMonThread);  g_hMonThread=nullptr;  }

    if(g_hEvHook){ UnhookWinEvent(g_hEvHook); g_hEvHook=nullptr; }

    if(g_hwndMain && IsWindow(g_hwndMain)){
        g_fsMode = FS_NONE;
        ApplyFS();
    }

    for(auto&[h,orig]:g_subclassed){
        SetWindowLongPtrW(h,GWLP_WNDPROC,(LONG_PTR)orig);
        ResetCaption(h);
        FrameRedraw(h);
    }
    g_subclassed.clear();

    for(HWND hw:g_darkened) if(IsWindow(hw)) ResetCaption(hw);
    g_darkened.clear();

    DestroyGDI();
    g_hwndMain=nullptr; g_hMenuSaved=nullptr; g_rcOrig={}; g_fsMode=FS_NONE;
    g_dragging=false; g_lastHotItem=-2;
}

extern "C" {
    __declspec(dllexport) void WINAPI PTDarkInstall()   { Install();   }
    __declspec(dllexport) void WINAPI PTDarkUninstall() { Uninstall(); }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if(reason==DLL_PROCESS_ATTACH){
        g_hModule=hModule;
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr,0,[](LPVOID)->DWORD{ Install(); return 0; },nullptr,0,nullptr);
    }
    else if(reason==DLL_PROCESS_DETACH){
        Uninstall();
    }
    return TRUE;
}
