// Native Board bridge for the single-plant/single-zombie LAN match milestone.
#include <windows.h>
#include <string.h>
#include "engine.h"
template<class T> static T& at(void* p,unsigned offset){return *(T*)((BYTE*)p+offset);}
extern "C" {
void pvzMakeBoard(void*);void pvzInitBoard(void*);void pvzStartPlaying(void*);void pvzKillSelector(void*);void pvzShowMenu(void*);
void* pvzAddPlant(void*,int,int);void* pvzAddZombie(void*,int);void* pvzAddShot(void*,int,int,int);
void pvzUpdateEffects(void*);void pvzUpdateLayers(void*);void pvzBottomGate();
void pvzDeleteQueue(void*);void pvzPlantDie(void*);void pvzZombieDie(void*);
int pvzGridX(void*,int,int);int pvzGridY(void*,int,int);
}
using Update=void (__thiscall*)(void*);
using Draw=void (__thiscall*)(void*,Graphics*);
static Update originalUpdate,originalLostFocus;
static Draw originalDraw;
static LanService* service;
static void (*logLine)(const char*);
static void* board;
static bool active=false,running=false,startRequest=false,leaveRequest=false,available=false;
static bool polling=false;
static unsigned callbackDepth=0;
struct CallbackGuard {CallbackGuard(){++callbackDepth;}~CallbackGuard(){--callbackDepth;}};
struct PollGuard {PollGuard(){polling=true;}~PollGuard(){polling=false;}};
static int savedMode=0,savedResult=0,localRole=0,winner=0;
static unsigned matchId=0,startedAt=0,lastPublish=0,lastIncome=0,lastCommand[2]={},requestSeq=0,remoteSeq=0;
static char status[128]="";
static MatchUI::Cell hoverCell={},pressedCell={};
static bool menuHover=false,menuPressed=false;
static Match::StateData state={};
struct Mirror {unsigned hostId,localId;void* object;};
static Mirror plants[Match::MaxPlants],zombies[Match::MaxZombies],shots[Match::MaxShots];
static void* app(){return *(void**)0x6a9ec0;}
static void say(const char* s){lstrcpynA(status,s,sizeof(status));if(logLine){logLine(s);logLine("\r\n");}}
static bool slot(DWORD table,unsigned index,DWORD expected,void* replacement){
    DWORD* p=(DWORD*)(table+index*4);if(*p!=expected)return false;
    DWORD old;if(!VirtualProtect(p,4,PAGE_READWRITE,&old))return false;
    InterlockedExchange((LONG*)p,(LONG)replacement);DWORD unused;VirtualProtect(p,4,old,&unused);return true;
}
static unsigned arrayOffset(int kind){return kind==0?0xac:kind==1?0x90:0xc8;}
static unsigned stride(int kind){return kind==0?0x14c:kind==1?0x15c:0x94;}
static unsigned deadOffset(int kind){return kind==0?0x141:kind==1?0xec:0x50;}
static bool exists(void* object,int kind,unsigned id){
    return object && at<unsigned>(object,stride(kind)-4)==id && (id&0xffff0000) && !at<bool>(object,deadOffset(kind));
}
static unsigned enumerate(int kind,void** result,unsigned cap){
    if(!board)return 0;unsigned off=arrayOffset(kind),step=stride(kind),count=0,used=at<unsigned>(board,off+4);
    if(used>1024)return 0;BYTE* base=at<BYTE*>(board,off);
    for(unsigned i=0;i<used && count<cap;++i){void* p=base+i*step;
        if((at<unsigned>(p,step-4)&0xffff0000) && !at<bool>(p,deadOffset(kind)))result[count++]=p;
    }
    return count;
}
static void removeObject(void* p,int kind){
    if(kind==0)pvzPlantDie(p);else if(kind==1)pvzZombieDie(p);else at<bool>(p,0x50)=true;
}
static void capture(){
    void* objects[1024];state.match=matchId;state.tick=at<unsigned>(board,0x5568);state.winner=winner;
    for(int kind=0;kind<3;++kind){
        unsigned cap=kind==0?Match::MaxPlants:kind==1?Match::MaxZombies:Match::MaxShots;
        unsigned n=enumerate(kind,objects,1024);
        // Keep the native arrays bounded to the protocol's complete-state capacity.
        for(unsigned i=cap;i<n;++i)removeObject(objects[i],kind);
        if(n>cap)n=cap;
        Match::Entity* out=kind==0?state.plant:kind==1?state.zombie:state.shot;
        if(kind==0)state.plants=n;else if(kind==1)state.zombies=n;else state.shots=n;
        for(unsigned i=0;i<n;++i){void* p=objects[i];
            int x=kind==0?at<int>(p,8)*1000:(int)(at<float>(p,kind==1?0x2c:0x30)*1000);
            int y=kind==0?at<int>(p,12)*1000:(int)(at<float>(p,kind==1?0x30:0x34)*1000);
            int hp=kind==0?at<int>(p,0x40):kind==1?at<int>(p,0xc8):0;
            if(hp<0)hp=0;
            out[i]={at<unsigned>(p,stride(kind)-4),at<int>(p,0x1c),kind==0?at<int>(p,0x28):0,hp,x,y,
                kind==0?at<int>(p,0x3c):kind==1?at<int>(p,0x28):0,kind==1?(at<bool>(p,0x51)?1:0):0};
        }
    }
}
static bool mirror(const Match::StateData& received){
    for(int kind=0;kind<3;++kind){
        Mirror* maps=kind==0?plants:kind==1?zombies:shots;
        const Match::Entity* data=kind==0?received.plant:kind==1?received.zombie:received.shot;
        unsigned cap=kind==0?Match::MaxPlants:kind==1?Match::MaxZombies:Match::MaxShots;
        unsigned n=kind==0?received.plants:kind==1?received.zombies:received.shots;
        // Release disappeared objects before allocating replacements (including reused lawn tiles).
        for(unsigned i=0;i<cap;++i)if(maps[i].hostId){
            bool keep=false;for(unsigned j=0;j<n;++j)if(maps[i].hostId==data[j].id)keep=true;
            if(!keep || !exists(maps[i].object,kind,maps[i].localId)){
                if(exists(maps[i].object,kind,maps[i].localId))removeObject(maps[i].object,kind);
                maps[i]={};
            }
        }
        pvzDeleteQueue(board);
        for(unsigned i=0;i<n;++i){
            const Match::Entity& e=data[i];Mirror* map=0;
            for(unsigned j=0;j<cap;++j)if(maps[j].hostId==e.id){map=&maps[j];break;}
            if(!map){
                for(unsigned j=0;j<cap;++j)if(!maps[j].hostId){map=&maps[j];break;}
                if(!map)return false;
                void* p=kind==0?pvzAddPlant(board,e.col,e.row):kind==1?pvzAddZombie(board,e.row):pvzAddShot(board,e.x/1000,e.y/1000,e.row);
                if(!p)return false;*map={e.id,at<unsigned>(p,stride(kind)-4),p};
            }
            void* p=map->object;at<int>(p,8)=e.x/1000;at<int>(p,12)=e.y/1000;
            if(kind==0){at<int>(p,0x40)=e.hp;}
            else{
                at<float>(p,kind==1?0x2c:0x30)=e.x/1000.0f;at<float>(p,kind==1?0x30:0x34)=e.y/1000.0f;
                if(kind==1){at<int>(p,0xc8)=e.hp;at<bool>(p,0x51)=(e.flags&1)!=0;}
            }
        }
    }
    state=received;winner=received.winner;return true;
}
static bool begin(unsigned id){
    if(active || !available || !app() || at<int>(app(),0x7fc)!=1)return false;
    LanSnapshot connection=lanSnapshot(service);if(connection.phase!=LAN_CONNECTED)return false;
    savedMode=at<int>(app(),0x7f8);savedResult=at<int>(app(),0x888);localRole=connection.role;matchId=id;
    state={};state.match=id;state.sun=300;state.brains=200;winner=0;
    memset(plants,0,sizeof(plants));memset(zombies,0,sizeof(zombies));memset(shots,0,sizeof(shots));
    hoverCell={};pressedCell={};menuHover=menuPressed=false;
    lastCommand[0]=lastCommand[1]=0;requestSeq=remoteSeq=0;
    at<int>(app(),0x888)=0;
    at<int>(app(),0x7f8)=1; // Day survival supplies a flat five-lane lawn, independent of profile progress.
    say("Match setup: removing selector and creating Board.");
    pvzKillSelector(app());pvzMakeBoard(app());board=at<void*>(app(),0x768);
    if(!board){at<int>(app(),0x7f8)=savedMode;at<int>(app(),0x888)=savedResult;return false;}
    say("Match setup: initializing the daylight lawn.");
    pvzInitBoard(board);
    say("Match setup: starting native level.");
    pvzStartPlaying(app());
    // The prototype has no lawnmower safety net; reaching the house ends this test.
    BYTE* mowerBase=at<BYTE*>(board,0x100);unsigned mowerCount=at<unsigned>(board,0x104);
    if(mowerBase && mowerCount<=32)for(unsigned i=0;i<mowerCount;++i){
        BYTE* mower=mowerBase+i*0x48;
        if(at<unsigned>(mower,0x44)&0xffff0000){at<bool>(mower,0x30)=true;at<bool>(mower,0x31)=false;}
    }
    pvzDeleteQueue(board);
    at<int>(board,0x559c)=100000;at<int>(board,0x5538)=100000;at<int>(board,0x5584)=0;
    at<int>(board,0x5748)=0;at<bool>(board,0x164)=false;
    // No native seed selection or progress saving: input is owned by the LAN HUD.
    if(at<void*>(board,0x144))at<int>(at<void*>(board,0x144),0x24)=0;
    active=true;running=false;startedAt=lastPublish=GetTickCount();lastIncome=0;
    say("Native LAN board created; waiting for readiness.");return true;
}
static void end(){
    if(!active)return;
    active=running=false;board=0;pvzShowMenu(app());at<int>(app(),0x7f8)=savedMode;at<int>(app(),0x888)=savedResult;
    say("Returned to the menu without saving the LAN board.");
}
static void command(unsigned action,int role,unsigned sequence){
    if(!active || !running || winner)return;
    if(role!=localRole){if(sequence<=remoteSeq)return;remoteSeq=sequence;}
    int row,col;if(!Match::parseAction(action,role,row,col))return;
    unsigned tick=GetTickCount();if(lastCommand[role] && tick-lastCommand[role]<300)return;
    if(role==0){
        void* objects[1024];unsigned n=enumerate(0,objects,1024);
        if(n>=Match::MaxPlants || state.sun<100)return;
        for(unsigned i=0;i<n;++i)if(at<int>(objects[i],0x1c)==row && at<int>(objects[i],0x28)==col)return;
        if(logLine){char line[96];wsprintfA(line,"Accepted plant command: row=%d col=%d role=%d\r\n",row,col,role);logLine(line);}
        if(!pvzAddPlant(board,col,row))return;
        state.sun-=100;
    }else{
        void* objects[1024];if(enumerate(1,objects,1024)>=Match::MaxZombies || state.brains<50)return;
        if(logLine){char line[80];wsprintfA(line,"Accepted zombie command: row=%d role=%d\r\n",row,role);logLine(line);}
        if(!pvzAddZombie(board,row))return;
        state.brains-=50;
    }
    lastCommand[role]=tick;
}
static void __fastcall update(void* self,void*){
    CallbackGuard guard;
    if(!active || self!=board){originalUpdate(self);return;}
    LanSnapshot connection=lanSnapshot(service);
    if(connection.phase!=LAN_CONNECTED){leaveRequest=true;at<int>(self,0x5748)=0;return;}
    bool simulate=connection.host && running && !winner;
    at<int>(self,0x5748)=0;at<bool>(self,0x164)=false;
    at<int>(self,0x559c)=100000;at<int>(self,0x5538)=100000;at<int>(self,0x5584)=0;
    if(simulate)originalUpdate(self);
    else {
        // Reanimations continue, but only the host updates combat and movement.
        void* effects=at<void*>(app(),0x820);
        if(effects)pvzUpdateEffects(effects);
        ++at<unsigned>(self,0x556c);
        pvzDeleteQueue(self);pvzUpdateLayers(self);
    }
    if(simulate){
        unsigned tick=GetTickCount();
        unsigned simulationTick=at<unsigned>(self,0x5568);
        if(simulationTick-lastIncome>=500){lastIncome=simulationTick;if(state.sun<9750)state.sun+=25;if(state.brains<9750)state.brains+=25;}
        void* objects[1024];unsigned n=enumerate(1,objects,1024);
        for(unsigned i=0;i<n;++i)if(at<int>(objects[i],0xc8)>0 && at<float>(objects[i],0x2c)<60.0f){
            winner=2;say("Zombies reached the house.");lanSendEvent(service,Match::Finish,2,matchId);break;
        }
        if(tick-lastPublish>=50 || winner){lastPublish=tick;capture();if(!lanPublishState(service,state)){say("Cannot publish native board state.");leaveRequest=true;}}
    }
}
static void __fastcall draw(void* self,void*,Graphics* g){CallbackGuard guard;originalDraw(self,g);if(active && self==board)engineDraw(g);}
static void __fastcall lostFocus(void* self,void*){
    if(!active)originalLostFocus(self); // LAN input has its own exit; avoid an inaccessible native pause modal.
}
static bool lostFocusSlot(){
    // Find the verified LawnApp virtual override rather than assuming a slot number.
    DWORD* table=*(DWORD**)app();
    for(unsigned i=0;i<160;++i)if(table[i]==0x44f460){
        originalLostFocus=(Update)0x44f460;
        return slot((DWORD)table,i,0x44f460,(void*)lostFocus);
    }
    return false;
}
static void __stdcall saveBoard(void* self){
    if(active && self==board){if(logLine)logLine("LAN board save suppressed.\r\n");return;}
    ((void (__stdcall*)(void*))0x408c30)(self);
}
static bool protectSaves(){
    const DWORD calls[]={0x40c03f,0x44ee55,0x44f25d,0x45333f};
    for(DWORD call:calls)if(*(BYTE*)call!=0xe8 || call+5+*(int*)(call+1)!=0x408c30)return false;
    for(DWORD call:calls){
        DWORD old;if(!VirtualProtect((void*)call,5,PAGE_EXECUTE_READWRITE,&old))return false;
        *(DWORD*)(call+1)=(DWORD)saveBoard-call-5;
        FlushInstructionCache(GetCurrentProcess(),(void*)call,5);
        DWORD unused;VirtualProtect((void*)call,5,old,&unused);
    }
    return true;
}
extern "C" int engineHideNativeUI(void* self){return active && self==board;}
static bool replaceNativeHUD(){
    const DWORD call=0x4172e7;
    if(*(BYTE*)call!=0xe8 || call+5+*(int*)(call+1)!=0x41a0a0)return false;
    DWORD old;if(!VirtualProtect((void*)call,5,PAGE_EXECUTE_READWRITE,&old))return false;
    *(DWORD*)(call+1)=(DWORD)pvzBottomGate-call-5;
    FlushInstructionCache(GetCurrentProcess(),(void*)call,5);
    DWORD unused;VirtualProtect((void*)call,5,old,&unused);return true;
}
bool engineInstall(LanService* s,void (*log)(const char*)){
    service=s;logLine=log;
    if(!s || *(DWORD*)(0x656ca8+22*4)!=0x415d40 || *(DWORD*)(0x656ca8+26*4)!=0x41acf0 ||
       memcmp((void*)0x40d120,"\x51\x53\x55",3) || memcmp((void*)0x40ddc0,"\x55\x8b\xec",3)){
        say("Native board signatures differ; match start disabled.");return false;
    }
    originalUpdate=(Update)0x415d40;originalDraw=(Draw)0x41acf0;
    available=protectSaves() && replaceNativeHUD() && slot(0x656ca8,22,0x415d40,(void*)update) && slot(0x656ca8,26,0x41acf0,(void*)draw) && lostFocusSlot();
    say(available?"Native board hooks installed (protocol 3).":"Cannot install native board hooks; match start disabled.");return available;
}
void enginePoll(){
    if(!service || polling || callbackDepth)return;
    PollGuard guard;
    LanSnapshot connection=lanSnapshot(service);
    if(active && connection.phase!=LAN_CONNECTED)leaveRequest=true;
    if(leaveRequest){leaveRequest=false;if(active){lanSendEvent(service,Match::Stop,0,matchId);end();}return;}
    if(startRequest){
        startRequest=false;
        if(connection.phase==LAN_CONNECTED && connection.host && !active){
            unsigned id=GetTickCount()|1;
            if(begin(id)){if(!lanSendEvent(service,Match::Start,id,0))leaveRequest=true;}
            else say("Cannot create native board; return to the main menu.");
        }
    }
    LanEvent event;
    while(lanPollEvent(service,&event)){
        if(event.type==Match::Start && !connection.host){
            if(begin(event.value)){if(!lanSendEvent(service,Match::Ready,matchId,0))leaveRequest=true;}
            else{lanSendEvent(service,Match::Stop,0,event.value);say("Client could not load the native board.");}
        }else if(event.type==Match::Ready && connection.host && active && event.value==matchId && !running){
            running=true;if(!lanSendEvent(service,Match::Go,matchId,0))leaveRequest=true;say("Both native boards ready; host simulation started.");
        }else if(event.type==Match::Go && !connection.host && active && event.value==matchId){running=true;say("Host started native simulation.");}
        else if(event.type==Match::Command && connection.host)command(event.value,1-localRole,event.nonce);
        else if(event.type==Match::Finish && !connection.host && active && event.nonce==matchId && event.value<=2){winner=(int)event.value;}
        else if(event.type==Match::Stop && active && event.nonce==matchId){end();return;}
    }
    if(active && !running && GetTickCount()-startedAt>15000){say("Board readiness timed out.");leaveRequest=true;}
    if(active && !connection.host){Match::StateData incoming={};if(lanReadState(service,&incoming) && incoming.match==matchId && incoming.tick>=state.tick){
        if(!mirror(incoming)){say("Cannot apply native mirror state.");leaveRequest=true;}
    }}
}
void engineRequestStart(){startRequest=true;}
bool engineActive(){return active;}
bool engineStarting(){return active && !running;}
void engineLeave(){leaveRequest=true;}
const char* engineStatus(){return status;}
const Match::StateData* engineState(){return &state;}
int engineRole(){return localRole;}
bool engineHover(MatchUI::Cell* cell){*cell=hoverCell;return running && !winner && hoverCell.row>=0;}
bool engineMenuHover(){return menuHover;}
static MatchUI::Cell cellAt(int x,int y){
    MatchUI::Cell cell;
    int bx=at<int>(board,0x30),by=at<int>(board,0x34);
    for(int r=0;r<5;++r)for(int c=0;c<9;++c){
        int gx=bx+pvzGridX(board,c,r),gy=by+pvzGridY(board,c,r);
        if(MatchUI::inside(x,y,gx,gy,80,100))return {r,c,gx,gy};
    }
    return cell;
}
bool engineInput(unsigned message,unsigned key,int x,int y){
    if(!active)return false;
    if(message==WM_KILLFOCUS){pressedCell={};hoverCell={};menuPressed=menuHover=false;return false;}
    if(message==WM_KEYDOWN && key==VK_ESCAPE){engineLeave();return true;}
    if(message==WM_MOUSEMOVE || message==WM_LBUTTONDOWN || message==WM_LBUTTONUP){
        menuHover=MatchUI::menuAt(x,y);hoverCell=cellAt(x,y);
    }
    if(message==WM_LBUTTONDOWN){pressedCell=hoverCell;menuPressed=menuHover;}
    if(message==WM_LBUTTONUP){
        bool leave=menuPressed && menuHover;
        bool place=pressedCell.row>=0 && hoverCell.row==pressedCell.row &&
            (localRole || hoverCell.col==pressedCell.col);
        pressedCell={};menuPressed=false;
        if(leave){engineLeave();return true;}
        if(!running || winner || !place)return true;
        unsigned action=Match::action(localRole?Match::Zombie:Match::Plant,hoverCell.row,localRole?8:hoverCell.col);
        if(lanSnapshot(service).host)command(action,localRole,0);
        else lanSendEvent(service,Match::Command,action,++requestSeq);
    }
    return message==WM_KEYDOWN || message==WM_KEYUP || message==WM_CHAR || message==WM_LBUTTONDOWN ||
        message==WM_LBUTTONUP || message==WM_RBUTTONDOWN || message==WM_RBUTTONUP || message==WM_MOUSEMOVE;
}
