#include <windows.h>
extern "C" DWORD WINAPI RunMenu(LPVOID);
static HANDLE logFile=INVALID_HANDLE_VALUE;
static void log(const char* text) {
    if(logFile==INVALID_HANDLE_VALUE) return;
    DWORD written; WriteFile(logFile,text,lstrlenA(text),&written,0); WriteFile(logFile,"\r\n",2,&written,0);
    FlushFileBuffers(logFile);
}
static BOOL CALLBACK inspect(HWND window,LPARAM parameter) {
    DWORD pid=0; GetWindowThreadProcessId(window,&pid);
    if(pid!=GetCurrentProcessId() || !IsWindowVisible(window)) return TRUE;
    char title[256], name[256]; GetWindowTextA(window,title,256); GetClassNameA(window,name,256);
    log("Visible game window:"); log(title); log(name);
    *(BOOL*)parameter=TRUE; return FALSE;
}
static DWORD WINAPI diagnostic(LPVOID) {
    char logPath[MAX_PATH];
    if(!GetEnvironmentVariableA("PVZMP_LOG_PATH",logPath,MAX_PATH)) lstrcpyA(logPath,"pvzmp-hook.log");
    logFile=CreateFileA(logPath,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
    log("PvZMP diagnostic DLL loaded (x86). Native widescreen menu module enabled.");
    for(int i=0;i<120;++i) {
        BOOL found=FALSE; EnumWindows(inspect,(LPARAM)&found);
        if(found) break;
        Sleep(500);
    }
    log("Diagnostic complete.");
    if(logFile!=INVALID_HANDLE_VALUE) CloseHandle(logFile);
    return 0;
}
extern "C" BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        HANDLE menu=CreateThread(0,0,RunMenu,instance,0,0);
        if(menu) CloseHandle(menu);
        HANDLE thread=CreateThread(0,0,diagnostic,0,0,0);
        if(thread) CloseHandle(thread);
    }
    return TRUE;
}
