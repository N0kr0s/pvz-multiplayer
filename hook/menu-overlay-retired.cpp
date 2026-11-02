#include <windows.h>
static HWND gameWindow;
static HINSTANCE module;
static HFONT titleFont;
static BOOL hovered=FALSE;
static BOOL CALLBACK findGame(HWND w,LPARAM) {
    DWORD pid=0; GetWindowThreadProcessId(w,&pid);
    if(pid==GetCurrentProcessId() && IsWindowVisible(w) && !GetWindow(w,GW_OWNER)) {
        RECT r; GetClientRect(w,&r);
        if(r.right>=640 && r.bottom>=480) { gameWindow=w; return FALSE; }
    }
    return TRUE;
}
static BOOL menuVisible() {
    // Steam GOTY 1.2.0.1096: validated against the embedded game's instructions.
    // Refuse other builds before reading the LawnApp singleton.
    BYTE expected[]={0xa1,0x50,0x1c,0x73,0x00,0x85,0xc0,0x75,0x03};
    BYTE actual[9]; SIZE_T read;
    if(!ReadProcessMemory(GetCurrentProcess(),(void*)0x4534a0,actual,9,&read) || read!=9) return FALSE;
    for(int i=0;i<9;++i) if(actual[i]!=expected[i]) return FALSE;
    DWORD app=0,scene=0;
    if(!ReadProcessMemory(GetCurrentProcess(),(void*)0x731c50,&app,4,&read) || !app) return FALSE;
    if(!ReadProcessMemory(GetCurrentProcess(),(void*)(app+0x91c),&scene,4,&read)) return FALSE;
    return scene==1;
}
static LRESULT CALLBACK plaque(HWND window,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if(msg==WM_MOUSEMOVE) {
        if(!hovered) { hovered=TRUE; TRACKMOUSEEVENT t={sizeof(t),TME_LEAVE,window,0}; TrackMouseEvent(&t); InvalidateRect(window,0,FALSE); }
        return 0;
    }
    if(msg==WM_MOUSELEAVE) { hovered=FALSE; InvalidateRect(window,0,FALSE); return 0; }
    if(msg==WM_LBUTTONUP) {
        MessageBoxA(gameWindow,"LAN multiplayer integration is in progress.\nThe native menu entry is installed; the Board/network connection is not installed yet.","Multiplayer",MB_OK|MB_ICONINFORMATION);
        return 0;
    }
    if(msg==WM_PAINT) {
        PAINTSTRUCT ps; HDC dc=BeginPaint(window,&ps); RECT r; GetClientRect(window,&r);
        HBRUSH stone=CreateSolidBrush(hovered?RGB(145,150,135):RGB(105,110,112));
        FillRect(dc,&r,stone); DeleteObject(stone);
        HPEN edge=CreatePen(PS_SOLID,3,RGB(47,48,54)); HGDIOBJ oldPen=SelectObject(dc,edge);
        HGDIOBJ oldBrush=SelectObject(dc,GetStockObject(NULL_BRUSH));
        RoundRect(dc,2,2,r.right-2,r.bottom-2,10,10);
        SelectObject(dc,oldBrush); SelectObject(dc,oldPen); DeleteObject(edge);
        SetBkMode(dc,TRANSPARENT); HGDIOBJ oldFont=SelectObject(dc,titleFont);
        SetTextColor(dc,RGB(17,18,24)); DrawTextA(dc,"MULTIPLAYER",-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        SelectObject(dc,oldFont); EndPaint(window,&ps); return 0;
    }
    return DefWindowProcA(window,msg,wp,lp);
}
extern "C" DWORD WINAPI RunMenu(LPVOID instance) {
    module=(HINSTANCE)instance;
    for(int i=0;i<240 && !gameWindow;++i) { EnumWindows(findGame,0); if(!gameWindow) Sleep(500); }
    if(!gameWindow) return 1;
    WNDCLASSA wc={}; wc.lpfnWndProc=plaque; wc.hInstance=module; wc.lpszClassName="PvZMPStonePlaque";
    wc.hCursor=LoadCursorA(0,IDC_HAND); RegisterClassA(&wc);
    HWND window=CreateWindowExA(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,"PvZMPStonePlaque","Multiplayer",WS_POPUP,
                              0,0,230,48,gameWindow,0,module,0);
    if(!window) return 2;
    int oldHeight=0;
    while(IsWindow(gameWindow)) {
        MSG msg; while(PeekMessageA(&msg,0,0,0,PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
        HWND foreground=GetForegroundWindow();
        if(!menuVisible() || IsIconic(gameWindow) || (foreground!=gameWindow && foreground!=window)) {
            ShowWindow(window,SW_HIDE); Sleep(50); continue;
        }
        RECT r; GetClientRect(gameWindow,&r); POINT origin={0,0}; ClientToScreen(gameWindow,&origin);
        int width=r.right,height=r.bottom;
        // The game preserves an 800x600 canvas inside its client area.
        if(width*3>height*4) { origin.x+=(width-height*4/3)/2; width=height*4/3; }
        else { origin.y+=(height-width*3/4)/2; height=width*3/4; }
        int h=height*43/600;
        if(oldHeight!=h) {
            if(titleFont) DeleteObject(titleFont);
            titleFont=CreateFontA(-h*3/5,0,0,0,FW_HEAVY,FALSE,FALSE,FALSE,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,FF_ROMAN,"Times New Roman");
            oldHeight=h; InvalidateRect(window,0,FALSE);
        }
        SetWindowPos(window,HWND_TOPMOST,origin.x+width*425/800,origin.y+height*446/600,width*246/800,h,SWP_NOACTIVATE|SWP_SHOWWINDOW);
        Sleep(50);
    }
    DestroyWindow(window); if(titleFont) DeleteObject(titleFont); return 0;
}
