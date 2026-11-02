#include <windows.h>
#include <tlhelp32.h>
static int fail(const char* message) { MessageBoxA(0,message,"PvZ Multiplayer loader",MB_ICONERROR); return 1; }
extern "C" void WINAPI Start() {
    char appid[32];
    if(!GetEnvironmentVariableA("SteamAppId",appid,sizeof(appid)) || lstrcmpA(appid,"3590")) {
        fail("Launch using the original Plants vs. Zombies Steam entry (AppID 3590), with steam-launch.sh in Launch Options. Do not add this loader as a non-Steam game.");
        ExitProcess(1);
    }
    char testInstance[16]={};
    bool testMode=GetEnvironmentVariableA("PVZMP_TEST_INSTANCE",testInstance,sizeof(testInstance)) &&
        (!lstrcmpA(testInstance,"host") || !lstrcmpA(testInstance,"client"));
    char directory[MAX_PATH], dll[MAX_PATH], exe[MAX_PATH];
    static char command[32768];
    GetModuleFileNameA(0,directory,MAX_PATH);
    char* last=directory; for(char* p=directory;*p;++p) if(*p=='\\' || *p=='/') last=p;
    *last=0;
    lstrcpyA(dll,directory); lstrcatA(dll,"\\pvzmp-hook.dll");
    // Always use the widescreen launcher: it applies runtime display patches.
    // Test instances isolate the prefix, not the executable launch chain.
    lstrcpyA(exe,directory); lstrcatA(exe,"\\..\\..\\PlantsVsZombies.exe");
    lstrcpyA(command,"\""); lstrcatA(command,exe); lstrcatA(command,"\"");
    // Preserve Steam's game arguments, including legacy PopCap Steam parameters.
    const char* args=GetCommandLineA();
    if(*args=='"') { ++args; while(*args && *args!='"') ++args; if(*args) ++args; }
    else while(*args && *args!=' ' && *args!='\t') ++args;
    if(lstrlenA(command)+lstrlenA(args)+1>=sizeof(command)) {
        fail("Game command line is too long."); ExitProcess(1);
    }
    lstrcatA(command,args);
    if(GetFileAttributesA(dll)==INVALID_FILE_ATTRIBUTES) { fail("pvzmp-hook.dll is missing."); ExitProcess(1); }
    STARTUPINFOA startup={}; startup.cb=sizeof(startup); PROCESS_INFORMATION process={};
    char gameDir[MAX_PATH]; lstrcpyA(gameDir,directory); lstrcatA(gameDir,"\\..\\..");
    char logPath[MAX_PATH]; lstrcpyA(logPath,gameDir); lstrcatA(logPath,"\\pvzmp-");
    if(testMode){lstrcatA(logPath,"test-");lstrcatA(logPath,testInstance);}else lstrcatA(logPath,"hook");
    lstrcatA(logPath,".log");
    SetEnvironmentVariableA("PVZMP_LOG_PATH",logPath);
    if(!CreateProcessA(exe,command,0,0,FALSE,0,0,gameDir,&startup,&process)) {
        fail("Could not start PlantsVsZombies.exe."); ExitProcess(1);
    }
    // The Steam PopCap wrapper spawns popcapgame*.exe. Inject into that
    // descendant only, after it creates its visible game window.
    DWORD descendants[64]={}; descendants[0]=process.dwProcessId; unsigned count=1;
    DWORD target=0;
    for(unsigned attempt=0;attempt<240 && !target;++attempt) {
        HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
        if(snapshot!=INVALID_HANDLE_VALUE) {
            PROCESSENTRY32 entry={}; entry.dwSize=sizeof(entry);
            if(Process32First(snapshot,&entry)) do {
                BOOL child=FALSE,known=FALSE;
                for(unsigned i=0;i<count;++i) {
                    if(entry.th32ParentProcessID==descendants[i]) child=TRUE;
                    if(entry.th32ProcessID==descendants[i]) known=TRUE;
                }
                if(child && !known && count<64) descendants[count++]=entry.th32ProcessID;
                if((child || known) && (!lstrcmpiA(entry.szExeFile,"pvzgame.exe") || !lstrcmpiA(entry.szExeFile,"popcapgame1.exe") ||
                   !lstrcmpiA(entry.szExeFile,"popcapgame2.exe") || !lstrcmpiA(entry.szExeFile,"popcapgame3.exe")))
                    target=entry.th32ProcessID;
            } while(Process32Next(snapshot,&entry));
            CloseHandle(snapshot);
        }
        if(!target) Sleep(500);
    }
    if(!target) {
        fail("The PopCap game child process was not found within 120 seconds.");
        CloseHandle(process.hThread); CloseHandle(process.hProcess); ExitProcess(1);
    }
    HANDLE game=OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|
                            PROCESS_VM_WRITE|PROCESS_VM_READ|SYNCHRONIZE,FALSE,target);
    SIZE_T size=lstrlenA(dll)+1;
    void* remote=game?VirtualAllocEx(game,0,size,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE):0;
    HANDLE thread=0;
    if(remote && WriteProcessMemory(game,remote,dll,size,0))
        thread=CreateRemoteThread(game,0,0,
            (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"),"LoadLibraryA"),remote,0,0);
    DWORD result=0;
    if(thread && WaitForSingleObject(thread,10000)==WAIT_OBJECT_0) GetExitCodeThread(thread,&result);
    if(!result) fail("Could not load the DLL into the PopCap game process.");
    if(thread) CloseHandle(thread);
    if(remote && result) VirtualFreeEx(game,remote,0,MEM_RELEASE);
    if(result) WaitForSingleObject(game,INFINITE);
    if(game) CloseHandle(game);
    CloseHandle(process.hThread); CloseHandle(process.hProcess); ExitProcess(result?0:1);
}
