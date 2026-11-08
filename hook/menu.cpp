// Widescreen 1.0 / original PvZ engine: render in Sexy's frame, never in a popup.
#include <windows.h>
#include "image-filter.h"
#include "lan.h"
#include "engine.h"
#include "road-layout.h"

struct Rect { int x,y,w,h; };
struct Color { int r,g,b,a; };
struct Graphics {
    void* vptr; void* dest;
    float tx,ty,sx,sy,ox,oy;
    Rect clip; Color color; void* font; int mode;
};
struct GameString { DWORD proxy; char text[16]; unsigned size,capacity; };
static_assert(sizeof(GameString)==28);
template<class T> static T& at(void* obj,unsigned offset) { return *(T*)((BYTE*)obj+offset); }
using Draw=void (__thiscall*)(void*,Graphics*);
using Update=void (__thiscall*)(void*);
using Hit=bool (__thiscall*)(void*,int,int);
static Draw originalOverlay,originalButtonDraw;
static Update originalUpdate;
static Hit originalHit;
static void* selector;
static HWND gameWindow;
static HHOOK inputHook;
static WNDPROC originalWindowProc;
static volatile LONG uiProgress=0,uiStage=0,lastWindowMessage=0,insideWindowProc=0;
static void progress(LONG stage){
    InterlockedExchange(&uiStage,stage);
    InterlockedExchange(&uiProgress,(LONG)GetTickCount());
}
static bool hover=false,down=false,showPanel=false;
static LanService* lan;
static bool boardReady=false;
static int page=0,hostRole=0,field=0,buttonHover=0,buttonDown=0;
static char joinIP[16]="",joinPort[6]="";
static char formError[80]="";
enum { BACK=1, HOST, JOIN, ROLE, CONNECT, DISCONNECT, START_MATCH };
struct PanelButton {int x,y,w,action;const char* label;};
static PanelButton panelButtons[5];
static int panelButtonCount;
static void addButton(int x,int y,int w,int action,const char* label) {
    panelButtons[panelButtonCount++]={x,y,w,action,label};
}
static void back() {
    if(page==1){page=0;formError[0]=0;}else showPanel=false;
    buttonDown=buttonHover=0;
}
static void activate(int action) {
    if(action==START_MATCH){engineRequestStart();showPanel=false;return;}
    if(action==BACK){back();return;}
    if(action==ROLE){hostRole=1-hostRole;return;}
    if(action==JOIN){page=1;field=0;formError[0]=0;return;}
    if(action==DISCONNECT){lanStop(lan);page=0;return;}
    if(action==HOST){lanHost(lan,hostRole);page=2;return;}
    if(action==CONNECT){
        unsigned port=0;
        for(const char* p=joinPort;*p;++p)port=port*10+(*p-'0');
        if(!joinIP[0] || port==0 || port>65535){lstrcpyA(formError,"Enter host IPv4 and port (1-65535)");return;}
        formError[0]=0;lanJoin(lan,joinIP,(unsigned short)port);page=2;
    }
}
static const float factor=.70f;
static int plaqueX=0,plaqueY=0;
static void record(const char* text) {
    OutputDebugStringA(text);
    char path[MAX_PATH];
    if(!GetEnvironmentVariableA("PVZMP_LOG_PATH",path,MAX_PATH-10)) return;
    lstrcatA(path,".menu");
    HANDLE file=CreateFileA(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
    if(file!=INVALID_HANDLE_VALUE) { DWORD n;WriteFile(file,text,lstrlenA(text),&n,0);CloseHandle(file); }
}
static bool menu() {
    DWORD app=*(DWORD*)0x6a9ec0;
    return app && at<int>((void*)app,0x7fc)==1;
}
// Install this subclass on the game's UI thread through the message hook.
// Forward native activation unchanged; record transitions for hang diagnosis.
static LRESULT CALLBACK windowProc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    if(message==WM_TIMER && wp==0x5056){enginePoll();return 0;}
    bool focusEvent=message==WM_ACTIVATEAPP || message==WM_ACTIVATE ||
        message==WM_SETFOCUS || message==WM_KILLFOCUS || message==WM_SIZE;
    if(message==WM_KILLFOCUS){
        engineInput(message,0,0,0);
        hover=down=false;buttonHover=buttonDown=0;ReleaseCapture();
    }
    if(focusEvent){
        char line[96];wsprintfA(line,"Window event begin: message=%u value=%u\r\n",message,(unsigned)wp);record(line);
        InterlockedExchange(&lastWindowMessage,(LONG)message);InterlockedIncrement(&insideWindowProc);
    }
    LRESULT result=CallWindowProcA(originalWindowProc,window,message,wp,lp);
    if(focusEvent){
        InterlockedDecrement(&insideWindowProc);
        char line[80];wsprintfA(line,"Window event end: message=%u\r\n",message);record(line);
    }
    return result;
}
static void installWindowProc() {
    if(originalWindowProc)return;
    WNDPROC previous=(WNDPROC)GetWindowLongPtrA(gameWindow,GWLP_WNDPROC);
    if(!previous)return;
    originalWindowProc=previous;
    SetLastError(0);
    LONG_PTR result=SetWindowLongPtrA(gameWindow,GWLP_WNDPROC,(LONG_PTR)windowProc);
    if(!result && GetLastError()){originalWindowProc=0;record("Focus window hook installation failed.\r\n");}
    else {record("Focus window hook installed on UI thread.\r\n");SetTimer(gameWindow,0x5056,20,0);}
}
static bool compact(void* button) {
    DWORD app=*(DWORD*)0x6a9ec0;
    return menu() && selector && at<void*>((void*)app,0x770)==selector &&
        (button==at<void*>(selector,0x94) || button==at<void*>(selector,0x98) || button==at<void*>(selector,0xb4));
}
static GameString string(const char* text) {
    GameString s={};s.capacity=15;
    while(text[s.size] && s.size<15) {s.text[s.size]=text[s.size];++s.size;}
    return s;
}
// Native string view for const-reference calls; long literals remain owned by this DLL.
static GameString stringView(const char* value) {
    unsigned length=lstrlenA(value);
    if(length<=15) return string(value);
    GameString s={};*(const char**)s.text=value;s.size=length;s.capacity=length;return s;
}
static void fill(Graphics* g,int x,int y,int w,int h,Color c) {
    Rect r={x+(int)g->tx,y+(int)g->ty,w,h};
    int right=r.x+r.w,bottom=r.y+r.h;
    if(r.x<g->clip.x) r.x=g->clip.x;
    if(r.y<g->clip.y) r.y=g->clip.y;
    if(right>g->clip.x+g->clip.w) right=g->clip.x+g->clip.w;
    if(bottom>g->clip.y+g->clip.h) bottom=g->clip.y+g->clip.h;
    r.w=right-r.x;r.h=bottom-r.y;
    if(r.w<=0 || r.h<=0) return;
    using Fn=void (__thiscall*)(void*,const Rect*,const Color*,int);
    ((Fn)(*(void***)g->dest)[2])(g->dest,&r,&c,0);
}
static int textWidth(void* font,const GameString& s) {
    using Fn=int (__thiscall*)(void*,const GameString*);
    return ((Fn)(*(void***)font)[7])(font,&s);
}
static void text(Graphics* g,void* font,const char* value,int x,int y,Color c) {
    if(!font) return;
    GameString s=stringView(value);
    using Fn=void (__thiscall*)(void*,Graphics*,int,int,const GameString*,const Color*,const Rect*);
    ((Fn)(*(void***)font)[10])(font,g,x,y,&s,&c,&g->clip);
}
struct SharedImageRef { void* shared; void* unshared; bool owned; BYTE padding[3]; };
static SharedImageRef plaqueReference={},labelReference={};
static void* plaqueImage;
static void* labelImage;
static bool spriteAttempted=false;
static void filterTexture(void* image,int width,int height) {
    if(!image) return;
    void** table=*(void***)image;
    // Verified DDImage virtual GetBits and MemoryImage::SetBits in this build.
    if(table[31]!=(void*)0x5712a0 || table[29]!=(void*)0x592e10) {
        record("Texture filter skipped: unsupported image methods.\r\n");return;
    }
    int sw=at<int>(image,0x24),sh=at<int>(image,0x28);
    if(sw<width || sh<height || sw>8192 || sh>8192) return;
    using GetBits=unsigned int* (__thiscall*)(void*);
    const unsigned int* pixels=((GetBits)table[31])(image);
    unsigned int* filtered=(unsigned int*)HeapAlloc(GetProcessHeap(),0,width*height*4);
    if(!filtered) return;
    if(reduceTexture(pixels,sw,sh,filtered,width,height)) {
        // SetBits copies into the game's allocation and invalidates native/GPU caches.
        using SetBits=void (__thiscall*)(void*,const unsigned int*,int,int,bool);
        ((SetBits)table[29])(image,filtered,width,height,true);
        record("Texture filtered and cached at its native UI size.\r\n");
    }
    HeapFree(GetProcessHeap(),0,filtered);
}
static void loadPlaque() {
    if(spriteAttempted) return;
    spriteAttempted=true;
    void* app=*(void**)0x6a9ec0;
    void* manager=at<void*>(app,0x634);
    if(!manager || *(DWORD*)manager!=0x675314) {record("Unexpected resource manager.\r\n");return;}
    GameString id=string("IMAGE_MP_PLAQUE");
    using Get=SharedImageRef* (__thiscall*)(void*,SharedImageRef*,const GameString*);
    ((Get)(*(void***)manager)[16])(manager,&plaqueReference,&id);
    plaqueImage=plaqueReference.unshared;
    if(!plaqueImage && plaqueReference.shared) plaqueImage=*(void**)plaqueReference.shared;
    id=string("IMAGE_MP_LABEL");
    ((Get)(*(void***)manager)[16])(manager,&labelReference,&id);
    labelImage=labelReference.unshared;
    if(!labelImage && labelReference.shared) labelImage=*(void**)labelReference.shared;
    filterTexture(plaqueImage,246,85);
    filterTexture(labelImage,222,64);
    record(plaqueImage?"PNG plaque loaded through ResourceManager.\r\n":"PNG plaque could not be loaded.\r\n");
}
static void drawTexture(Graphics* g,void* image,int x,int y,int w,int h,Color tint,int mode=0) {
    if(!image) return;
    Rect source={0,0,at<int>(image,0x24),at<int>(image,0x28)};
    Rect target={x+(int)g->tx,y+(int)g->ty,w,h};
    using Stretch=void (__thiscall*)(void*,void*,const Rect*,const Rect*,const Rect*,const Color*,int,bool);
    ((Stretch)(*(void***)g->dest)[12])(g->dest,image,&target,&source,&g->clip,&tint,mode,false);
}
static SharedImageRef dialogRefs[13]={};
static void* dialogImages[13]={};
static bool dialogLoaded=false;
static void loadDialog() {
    if(dialogLoaded) return;
    dialogLoaded=true;
    const char* ids[]={"IMAGE_DIALOG_TOPLEFT","IMAGE_DIALOG_TOPMIDDLE","IMAGE_DIALOG_TOPRIGHT",
        "IMAGE_DIALOG_CENTERLEFT","IMAGE_DIALOG_CENTERMIDDLE","IMAGE_DIALOG_CENTERRIGHT",
        "IMAGE_DIALOG_BOTTOMLEFT","IMAGE_DIALOG_BOTTOMMIDDLE","IMAGE_DIALOG_BOTTOMRIGHT",
        "IMAGE_BUTTON_LEFT","IMAGE_BUTTON_MIDDLE","IMAGE_BUTTON_RIGHT","IMAGE_DIALOG_HEADER"};
    void* manager=at<void*>(*(void**)0x6a9ec0,0x634);
    using Get=SharedImageRef* (__thiscall*)(void*,SharedImageRef*,const GameString*);
    for(int i=0;i<13;++i) {
        GameString id=stringView(ids[i]);
        ((Get)(*(void***)manager)[16])(manager,&dialogRefs[i],&id);
        dialogImages[i]=dialogRefs[i].unshared;
        if(!dialogImages[i] && dialogRefs[i].shared) dialogImages[i]=*(void**)dialogRefs[i].shared;
    }
}
static void centered(Graphics* g,void* font,const char* value,int y,Color color) {
    if(!font) return;
    GameString label=stringView(value);
    text(g,font,value,534-textWidth(font,label)/2,y,color);
}
static void panelButton(Graphics* g,const PanelButton& b) {
    int offset=buttonDown==b.action?1:0;
    int x=b.x+offset,y=b.y+offset;
    int lw=at<int>(dialogImages[9],0x24),rw=at<int>(dialogImages[11],0x24);
    void* font=*(void**)0x6a7314;
    for(int pass=0;pass<(buttonHover==b.action?2:1);++pass) {
        Color c=pass?Color{38,38,38,255}:Color{255,255,255,255};
        drawTexture(g,dialogImages[9],x,y,lw,33,c,pass);
        drawTexture(g,dialogImages[10],x+lw,y,b.w-lw-rw,33,c,pass);
        drawTexture(g,dialogImages[11],x+b.w-rw,y,rw,33,c,pass);
    }
    GameString label=stringView(b.label);
    text(g,font,b.label,x+(b.w-textWidth(font,label))/2,y+24,{255,245,218,255});
}
static void editField(Graphics* g,int which,int y,const char* label,const char* value) {
    Color cream={255,245,218,255};
    text(g,*(void**)0x6a7224,label,335,y+22,cream);
    fill(g,410,y,300,31,field==which?Color{155,153,180,255}:Color{88,88,110,255});
    fill(g,412,y+2,296,27,{34,35,51,255});
    char content[24];lstrcpyA(content,value);
    if(field==which && (GetTickCount()/500)%2==0)lstrcatA(content,"_");
    text(g,*(void**)0x6a7224,content,420,y+22,cream);
}
static void dialog(Graphics* g) {
    loadDialog();panelButtonCount=0;
    fill(g,0,0,1067,600,{0,0,0,90});
    const int x=260,y=145,w=548,h=330;
    const int left=107,right=120,top=97,bottom=114;
    const int xs[]={x,x+left,x+w-right},ys[]={y,y+top,y+h-bottom};
    const int widths[]={left,w-left-right,right},heights[]={top,h-top-bottom,bottom};
    for(int row=0;row<3;++row) for(int col=0;col<3;++col)
        drawTexture(g,dialogImages[row*3+col],xs[col],ys[row],widths[col],heights[row],{255,255,255,255});
    if(dialogImages[12]) {
        int width=at<int>(dialogImages[12],0x24),height=at<int>(dialogImages[12],0x28);
        drawTexture(g,dialogImages[12],534-width/2,130,width,height,{255,255,255,255});
    }
    const Color cream={255,245,218,255};
    void* body=*(void**)0x6a7224;
    centered(g,*(void**)0x6a7714,"MULTIPLAYER",220,cream);
    if(!lan){centered(g,body,"Network initialization failed. Restart the game.",282,cream);}
    else if(page==0){
        centered(g,body,"Play with a friend on your local network",260,cream);
        addButton(329,284,195,HOST,"HOST");addButton(544,284,195,JOIN,"JOIN");
        addButton(394,334,280,ROLE,hostRole?"HOST: ZOMBIES":"HOST: PLANTS");
        centered(g,body,"The other player gets the opposite side.",395,cream);
    }else if(page==1){
        centered(g,body,"Enter the address shown on the host screen",252,cream);
        editField(g,0,268,"IP",joinIP);editField(g,1,310,"PORT",joinPort);
        if(formError[0])centered(g,body,formError,362,{255,190,140,255});
        addButton(424,373,220,CONNECT,"CONNECT");
    }else{
        LanSnapshot state=lanSnapshot(lan);
        char line[160];
        const char* status="Starting network...";
        if(state.phase==LAN_LISTENING)status="Waiting for the other player...";
        if(state.phase==LAN_CONNECTING)status="Connecting to host...";
        if(state.phase==LAN_HANDSHAKE)status="Checking lobby version...";
        if(state.phase==LAN_CONNECTED)status="CONNECTED - TWO PLAYERS";
        if(state.phase==LAN_ERROR)status="Connection ended";
        centered(g,body,status,252,state.phase==LAN_CONNECTED?Color{175,255,145,255}:cream);
        if(state.host && state.port){
            // Show the preferred LAN address, without a confusing VPN/address list.
            char ip[49];lstrcpynA(ip,state.addresses,49);
            wsprintfA(line,"Host IP: %s",ip);centered(g,body,line,280,cream);
            wsprintfA(line,"Port: %u   /   You: %s",state.port,state.role?"Zombies":"Plants");centered(g,body,line,309,cream);
        }else if(state.peer[0]){
            wsprintfA(line,"Host: %s : %u",state.peer,state.port);centered(g,body,line,280,cream);
            if(state.phase==LAN_CONNECTED){wsprintfA(line,"You: %s   /   Host: %s",state.role?"Zombies":"Plants",state.role?"Plants":"Zombies");centered(g,body,line,309,cream);}
        }
        if(state.phase==LAN_CONNECTED && !boardReady)centered(g,body,"Native match unavailable; check pvzmp-hook.log.menu",337,{255,190,140,255});
        else if(state.phase==LAN_ERROR)centered(g,body,state.message,337,{255,190,140,255});
        else if(state.phase==LAN_CONNECTED){
            wsprintfA(line,state.host?"Ping: %u ms. Start when both players are ready.":"Ping: %u ms. Waiting for host to start.",state.rtt);centered(g,body,line,337,cream);
        }else if(state.phase==LAN_LISTENING)centered(g,body,"Share your IP and port with your friend.",337,cream);
        if(state.phase==LAN_CONNECTED && state.host && boardReady){
            addButton(329,365,195,START_MATCH,"START");addButton(544,365,195,DISCONNECT,"DISCONNECT");
        }else addButton(414,365,240,DISCONNECT,state.phase==LAN_ERROR?"TRY AGAIN":"DISCONNECT");
    }
    addButton(454,420,160,BACK,"BACK");
    for(int i=0;i<panelButtonCount;++i)panelButton(g,panelButtons[i]);
}
// Use the original seed-bank wood, resource icons and native I, Zombie cards.
extern "C" void pvzDrawSeedPacket(Graphics*,float,float,int,int);
extern "C" void pvzDrawSeedPacketNoCost(Graphics*,float,float,int,int);
extern "C" void pvzSetButtonLabel(void*,const GameString*);
extern "C" {int pvzPortraitOverride=-1;}
extern int engineTypeCost(int);
void engineSetReadyLabel(void* button,const char* value){if(button){GameString label=stringView(value);pvzSetButtonLabel(button,&label);}}
// Reuse the original packet, cached zombie renderer, tint and small price font.
// Only the portrait's zombie type is overridden during this synchronous draw.
extern "C" void engineCustomCard(Graphics* g,float x,float y,int card,int,float,int,int,int gray){
    int type=NativeCatalog::zombieType(card);if(type<0)return;
    int base=type==32?73:(type==13 || type==22)?70:type==9?72:60;
    pvzPortraitOverride=type;pvzDrawSeedPacketNoCost(g,x,y,base,gray);pvzPortraitOverride=-1;
    char amount[16];wsprintfA(amount,"%d",engineTypeCost(card));GameString label=stringView(amount);void* font=*(void**)0x6a7498;
    text(g,font,amount,(int)x+25-textWidth(font,label)/2,(int)y+68,{35,27,16,255});
}
static void drawCard(Graphics* g,float x,float y,int card,int gray){
    if(card>=100)engineCustomCard(g,x,y,card,-1,0,1,0,gray);else pvzDrawSeedPacket(g,x,y,card,gray);
}
static SharedImageRef hudRefs[4]={};
static void* hudImages[4]={};
static bool hudLoaded=false;
static void loadHUD(){
    if(hudLoaded)return;hudLoaded=true;
    const char* ids[]={"IMAGE_SEEDBANK","IMAGE_SUNBANK","IMAGE_BRAIN","IMAGE_SEEDCHOOSER_BACKGROUND"};
    void* manager=at<void*>(*(void**)0x6a9ec0,0x634);
    using Get=SharedImageRef* (__thiscall*)(void*,SharedImageRef*,const GameString*);
    for(int i=0;i<4;++i){GameString id=stringView(ids[i]);
        ((Get)(*(void***)manager)[16])(manager,&hudRefs[i],&id);
        hudImages[i]=hudRefs[i].unshared;
        if(!hudImages[i] && hudRefs[i].shared)hudImages[i]=*(void**)hudRefs[i].shared;
    }
}
static void region(Graphics* g,void* image,Rect source,Rect target){
    if(!image)return;
    Color white={255,255,255,255};
    using Stretch=void (__thiscall*)(void*,void*,const Rect*,const Rect*,const Rect*,const Color*,int,bool);
    ((Stretch)(*(void***)g->dest)[12])(g->dest,image,&target,&source,&g->clip,&white,0,false);
}
extern "C" void pvzDrawRoadZombie(void*,Graphics*);
static SharedImageRef roadRef={},roadBushRefs[6]={};static void* roadImage=0;static void* roadBushes[6]={};static bool roadLoaded=false;
static void loadRoad(){
    if(roadLoaded)return;roadLoaded=true;void* manager=at<void*>(*(void**)0x6a9ec0,0x634);
    using Get=SharedImageRef* (__thiscall*)(void*,SharedImageRef*,const GameString*);
    GameString id=stringView("IMAGE_MP_ROAD");((Get)(*(void***)manager)[16])(manager,&roadRef,&id);
    roadImage=roadRef.unshared;if(!roadImage && roadRef.shared)roadImage=*(void**)roadRef.shared;
    // These are the widescreen launcher's original day-bush resource IDs.
    const char* bushIds[]={"IMAGE_B_BASE","IMAGE_BD0","IMAGE_BD1","IMAGE_BD2","IMAGE_BD3","IMAGE_BD4"};
    for(int i=0;i<6;++i){id=stringView(bushIds[i]);((Get)(*(void***)manager)[16])(manager,&roadBushRefs[i],&id);
        roadBushes[i]=roadBushRefs[i].unshared;
        if(!roadBushes[i] && roadBushRefs[i].shared)roadBushes[i]=*(void**)roadBushRefs[i].shared;
    }
}
static void drawRoadBushes(Graphics* g,int left){
    // A single mirrored native hedge stack, just like the widescreen border.
    // Draw whole alpha sprites: no soil mask, row strips, repeated tiles or PNG edits.
    Color white={255,255,255,255};Rect target={left-160,0,500,600};
    using Mirror=void (__thiscall*)(void*,void*,const Rect*,const Rect*,const Rect*,const Color*,int,bool);
    for(int i=0;i<6;++i)if(roadBushes[i]){
        Rect source={0,0,at<int>(roadBushes[i],0x24),at<int>(roadBushes[i],0x28)};
        ((Mirror)(*(void***)g->dest)[16])(g->dest,roadBushes[i],&target,&source,&g->clip,&white,0,false);
    }
}
void engineDrawWorld(void* self,Graphics* g,void* original){
    float tx=g->tx,ty=g->ty,offset=engineCameraOffset();Rect clip=g->clip;
    g->tx=tx-offset;((Draw)original)(self,g);g->clip=clip;g->tx=0;g->ty=0;
    if(offset>0){
        loadRoad();int left=1067-(int)offset;
        // Unmodified user panorama, followed by separate native bush layers.
        drawTexture(g,roadImage,left,0,1067,600,{255,255,255,255});
        for(int row=0;row<5;++row)for(unsigned i=0;i<engineRoadCount();++i){float x;int lane;void* p=engineRoadVisual(i,&x,&lane);if(!p || lane!=row)continue;
            float px=at<float>(p,0x2c),py=at<float>(p,0x30);int ix=at<int>(p,8),iy=at<int>(p,12),height=at<int>(p,0x64);
            at<int>(p,0x64)=0;
            at<float>(p,0x2c)=x;at<float>(p,0x30)=(float)RoadLayout::bodyY(lane);at<int>(p,8)=(int)x;at<int>(p,12)=RoadLayout::bodyY(lane);
            g->tx=(float)left+x;g->ty=(float)RoadLayout::bodyY(lane);g->clip=clip;
            pvzDrawRoadZombie(p,g);
            at<float>(p,0x2c)=px;at<float>(p,0x30)=py;at<int>(p,8)=ix;at<int>(p,12)=iy;at<int>(p,0x64)=height;
        }
        g->tx=g->ty=0;g->clip=clip;drawRoadBushes(g,left);
    }
    g->tx=tx;g->ty=ty;g->clip=clip;
}
static void __stdcall drawCoin(void* coin,Graphics* g){
    if(!engineCoinVisible(coin))return;
    engineRecordCoinDraw(coin,g->tx,g->ty);
    ((void (__stdcall*)(void*,Graphics*))0x431810)(coin,g);
}
bool engineInstallCoinDraw(){
    const DWORD call=0x41718f;
    if(*(BYTE*)call!=0xe8 || call+5+*(int*)(call+1)!=0x431810)return false;
    DWORD old;if(!VirtualProtect((void*)call,5,PAGE_EXECUTE_READWRITE,&old))return false;
    *(DWORD*)(call+1)=(DWORD)drawCoin-call-5;FlushInstructionCache(GetCurrentProcess(),(void*)call,5);
    DWORD unused;VirtualProtect((void*)call,5,old,&unused);return true;
}
void engineChooserOverlay(Graphics* g){
    loadHUD();
    region(g,hudImages[3],{70,4,325,25},{70+(int)g->tx,91+(int)g->ty,325,25});
    void* font=*(void**)0x6a7314;GameString label=stringView("CHOOSE YOUR ZOMBIES");
    text(g,font,"CHOOSE YOUR ZOMBIES",229-textWidth(font,label)/2,110,{255,245,218,255});
}
void engineDraw(Graphics* g) {
    if(enginePreparing())return; // The stock chooser and camera draw themselves.
    progress(4);float tx=g->tx,ty=g->ty;g->tx=g->ty=0;
    loadDialog();loadHUD();const Match::StateData* state=engineState();
    if(state->winner){
        if(engineResultReady()){
            fill(g,0,0,1067,600,{0,0,0,110});
            const int xs[]={300,407,647},ys[]={170,267,336},ws[]={107,240,120},hs[]={97,69,114};
            for(int row=0;row<3;++row)for(int col=0;col<3;++col)drawTexture(g,dialogImages[row*3+col],xs[col],ys[row],ws[col],hs[row],{255,255,255,255});
            bool won=engineRole()==1;const Color cream={255,245,218,255};
            centered(g,*(void**)0x6a7714,won?"VICTORY":"DEFEAT",250,cream);
            centered(g,*(void**)0x6a7224,won?"The garden is yours!":"The zombies reached your house.",318,cream);
            int hover=buttonHover;buttonHover=engineResultHover()?98:0;
            panelButton(g,{414,384,240,98,"BACK TO LOBBY"});buttonHover=hover;
        }
        g->tx=tx;g->ty=ty;progress(0);return;
    }
    bool zombie=engineRole()==1;int resource=zombie?state->brains:state->sun;
    const Color ink={35,27,16,255};void* font=*(void**)0x6a7498; // native small numeric font
    unsigned count=engineCardCount();int end=114+(int)count*54;
    if(hudImages[0]){
        int w=at<int>(hudImages[0],0x24),h=at<int>(hudImages[0],0x28);
        region(g,hudImages[0],{76,0,w-92,h},{76,0,end-76,78});
        region(g,hudImages[0],{w-16,0,16,h},{end,0,16,78});
    }
    drawTexture(g,hudImages[1],8,0,70,78,{255,255,255,255});
    if(zombie){region(g,hudImages[0],{100,8,60,52},{16,5,54,46});drawTexture(g,hudImages[2],24,8,40,40,{255,255,255,255});}
    char amount[24];wsprintfA(amount,"%d",resource);GameString label=stringView(amount);
    text(g,font,amount,43-textWidth(font,label)/2,72,ink);
    for(unsigned i=0;i<count;++i){int x=98+(int)i*54,cost=engineCardCost(i);unsigned wait=engineCardWait(i);
        if(engineSelected()==(int)i)fill(g,x-2,2,54,74,{255,238,135,220});
        drawCard(g,(float)x,4.0f,engineCardType(i),resource>=cost && !wait?255:100);
        if(wait){int cover=wait/20;if(cover>54)cover=54;fill(g,x+2,5,46,cover,{20,23,30,130});}
        // Price is drawn by native DrawSeedPacket using the original small font.
    }
    panelButton(g,{953,90,94,97,engineRoadView()?"<":" >"});
    MatchUI::Cell cell;
    if(engineHover(&cell)){
        int left=cell.x,width=zombie?RoadLayout::CellWidth:80;
        int height=zombie?RoadLayout::RowHeight:100;
        int chosen=engineSelected();bool allowed=chosen>=0 && resource>=engineCardCost(chosen) && !engineCardWait(chosen);
        Color edge=allowed?Color{255,238,135,125}:Color{230,90,75,130};
        fill(g,left+2,cell.y+2,width-4,2,edge);fill(g,left+2,cell.y+height-4,width-4,2,edge);
        fill(g,left+2,cell.y+4,2,height-8,edge);fill(g,left+width-4,cell.y+4,2,height-8,edge);
    }
    int savedHover=buttonHover;buttonHover=engineMenuHover()?99:0;
    panelButton(g,{MatchUI::MenuX,MatchUI::MenuY,MatchUI::MenuW,99,"MENU"});buttonHover=savedHover;
    if(engineStarting() || state->winner)panelButton(g,{414,250,240,98,engineStarting()?"WAITING...":"ZOMBIES WIN"});
    g->tx=tx;g->ty=ty;progress(0);
}
static void stone(Graphics* g) {
    loadPlaque();
    if(!plaqueImage) return;
    drawTexture(g,plaqueImage,plaqueX+(down?1:0),plaqueY+(down?1:0),246,85,{255,255,255,255});
    // Additive tint brightens the exact same texture, preserving its contour and alpha.
    if(hover) drawTexture(g,plaqueImage,plaqueX,plaqueY,246,85,{48,48,48,255},1);
    // Lettering is its own cached alpha texture and stays dark when the stone brightens.
    if(labelImage) drawTexture(g,labelImage,plaqueX+12+(down?1:0),plaqueY+10+(down?1:0),222,64,{255,255,255,255});
}
static void __fastcall buttonDraw(void* self,void*,Graphics* g) {
    if(!compact(self)) { originalButtonDraw(self,g);return; }
    float sy=g->sy,oy=g->oy;
    g->sy*=factor;g->oy=g->ty;
    originalButtonDraw(self,g);
    g->sy=sy;g->oy=oy;
}
static bool __fastcall buttonHit(void* self,void*,int x,int y) {
    return originalHit(self,x,compact(self)?(int)(y/factor):y);
}
static void __fastcall update(void* self,void*) {
    progress(1);
    selector=self;
    originalUpdate(self);
    progress(0);
    const unsigned offsets[3]={0x94u,0x98u,0xb4u};
    for(unsigned offset : offsets) {
        void* b=at<void*>(self,offset);
        void* image=at<void*>(b,0xb0);
        if(image) at<int>(b,0x3c)=(int)(at<int>(image,0x28)*factor);
    }
    void* last=at<void*>(self,0xb4);
    plaqueX=at<int>(last,0x30)+2;
    plaqueY=at<int>(last,0x34)+49;
    static LanPhase lastPhase=LAN_IDLE;
    if(lan){
        LanSnapshot state=lanSnapshot(lan);
        if(state.phase!=lastPhase){
            char message[384];
            wsprintfA(message,"LAN phase=%d host=%d role=%d port=%u IP=%s peer=%s %s\r\n",
                (int)state.phase,state.host?1:0,state.role,state.port,state.addresses,state.peer,state.message);
            record(message);lastPhase=state.phase;
        }
    }
    if(!menu()) showPanel=false;
}
static void __fastcall drawOverlay(void* self,void*,Graphics* g) {
    progress(2);
    originalOverlay(self,g);
    progress(0);
    if(!menu() || selector!=self) return;
    progress(3);
    stone(g);
    if(showPanel) dialog(g);
    progress(0);
}
static bool contains(int x,int y) {
    x-=plaqueX;y-=plaqueY;
    return x>=0 && x<244 && y>=x*34/246 && y<x*34/246+50;
}
static LRESULT CALLBACK input(int code,WPARAM wp,LPARAM lp) {
    if(code>=0)installWindowProc();
    if(code>=0 && wp==PM_REMOVE && engineActive()){
        MSG* msg=(MSG*)lp;
        if(msg->hwnd==gameWindow){
            RECT r;GetClientRect(gameWindow,&r);
            int x=(short)LOWORD(msg->lParam),y=(short)HIWORD(msg->lParam);
            if(r.right && r.bottom){x=x*MatchUI::Width/r.right;y=y*MatchUI::Height/r.bottom;}
            if(engineInput(msg->message,(unsigned)msg->wParam,x,y))msg->message=WM_NULL;
        }
    }
    if(code>=0 && wp==PM_REMOVE && menu()) {
        MSG* msg=(MSG*)lp;
        if(msg->hwnd==gameWindow && showPanel && (msg->message==WM_KEYDOWN || msg->message==WM_KEYUP || msg->message==WM_CHAR)) {
            if(msg->message==WM_KEYDOWN){
                if(msg->wParam==VK_ESCAPE)back();
                else if(page==1 && msg->wParam==VK_TAB)field=1-field;
                else if(page==1 && msg->wParam==VK_RETURN)activate(CONNECT);
                else if(page==1){
                    // The message hook consumes keydown, so explicitly translate text keys.
                    TranslateMessage(msg);
                }
            }else if(msg->message==WM_CHAR && page==1){
                char* value=field?joinPort:joinIP;
                int length=lstrlenA(value),limit=field?5:15;
                unsigned c=(unsigned)msg->wParam;
                if(c==8 && length)value[length-1]=0;
                else if(length<limit && ((c>='0' && c<='9') || (!field && c=='.'))){value[length]=(char)c;value[length+1]=0;}
            }
            // TranslateMessage cannot leak modal input into the original selector.
            msg->message=WM_NULL;
        }
        if(msg->hwnd==gameWindow && (msg->message==WM_MOUSEMOVE || msg->message==WM_LBUTTONDOWN || msg->message==WM_LBUTTONUP)) {
            RECT r;GetClientRect(gameWindow,&r);
            int x=(short)LOWORD(msg->lParam),y=(short)HIWORD(msg->lParam);
            if(r.right && r.bottom) { x=x*MatchUI::Width/r.right;y=y*MatchUI::Height/r.bottom; }
            hover=contains(x,y);
            if(showPanel) {
                buttonHover=0;
                for(int i=0;i<panelButtonCount;++i){
                    const PanelButton& b=panelButtons[i];
                    if(x>=b.x && x<b.x+b.w && y>=b.y && y<b.y+33)buttonHover=b.action;
                }
                if(msg->message==WM_LBUTTONDOWN){
                    buttonDown=buttonHover;
                    if(page==1 && x>=410 && x<710){if(y>=268 && y<299)field=0;else if(y>=310 && y<341)field=1;}
                }
                if(msg->message==WM_LBUTTONUP){
                    int action=buttonDown;buttonDown=0;
                    if(action && action==buttonHover)activate(action);
                }
                msg->message=WM_NULL;
            } else if(hover) {
                if(msg->message==WM_LBUTTONDOWN) down=true;
                if(msg->message==WM_LBUTTONUP) { if(down) {showPanel=true;buttonHover=buttonDown=0;}down=false; }
                // Preserve movement for the original widget manager, consume new-button clicks.
                if(msg->message!=WM_MOUSEMOVE) msg->message=WM_NULL;
            } else if(msg->message==WM_LBUTTONUP) down=false;
        }
    }
    return CallNextHookEx(inputHook,code,wp,lp);
}
static BOOL CALLBACK findGame(HWND w,LPARAM) {
    DWORD pid;GetWindowThreadProcessId(w,&pid);
    if(pid==GetCurrentProcessId() && IsWindowVisible(w) && !GetWindow(w,GW_OWNER)) {
        RECT r;GetClientRect(w,&r);
        if(r.right>=640 && r.bottom>=480) {gameWindow=w;return FALSE;}
    }
    return TRUE;
}
static bool writeSlot(DWORD table,int slot,DWORD expected,void* value) {
    DWORD* target=(DWORD*)(table+slot*4);
    if(*target!=expected) return false;
    DWORD old;if(!VirtualProtect(target,4,PAGE_READWRITE,&old)) return false;
    InterlockedExchange((LONG*)target,(LONG)value);
    DWORD unused;VirtualProtect(target,4,old,&unused);return true;
}
extern "C" DWORD WINAPI RunMenu(LPVOID instance) {
    record("PvZMP: checking native widescreen menu signatures.\r\n");
    // Verify all slots before modifying any of them. Widescreen patches do not replace these slots.
    if(*(DWORD*)0x6655f0!=0x44ab50 || *(DWORD*)0x665598!=0x44b2a0 ||
       *(DWORD*)0x658778!=0x448790 || *(DWORD*)0x658800!=0x448b70) {record("Unsupported menu vtables; no hooks installed.\r\n");return 1;}
    originalOverlay=(Draw)0x44ab50;originalUpdate=(Update)0x44b2a0;
    originalButtonDraw=(Draw)0x448790;originalHit=(Hit)0x448b70;
    for(int i=0;i<240 && !gameWindow;++i) {EnumWindows(findGame,0);if(!gameWindow) Sleep(500);}
    if(!gameWindow) {record("No game window found.\r\n");return 2;}
    if(!writeSlot(0x665540,44,0x44ab50,(void*)drawOverlay) ||
       !writeSlot(0x665540,22,0x44b2a0,(void*)update) ||
       !writeSlot(0x658710,26,0x448790,(void*)buttonDraw) ||
       !writeSlot(0x658710,60,0x448b70,(void*)buttonHit)) return 3;
    lan=lanCreate();
    record(lan?"Native LAN worker initialized.\r\n":"Native LAN initialization failed.\r\n");
    boardReady=engineInstall(lan,record);
    inputHook=SetWindowsHookExA(WH_GETMESSAGE,input,(HINSTANCE)instance,GetWindowThreadProcessId(gameWindow,0));
    record("PvZMP: native widescreen menu hooks installed.\n");
    if(!inputHook)return 4;
    PostMessageA(gameWindow,WM_NULL,0,0);
    bool stalled=false;
    while(IsWindow(gameWindow)){
        Sleep(1000);
        unsigned tick=(unsigned)InterlockedCompareExchange(&uiProgress,0,0);
        bool late=tick && GetTickCount()-tick>3000;
        if(late && !stalled){
            char line[180];
            wsprintfA(line,"UI heartbeat absent >3s: stage=%d lastWindowMessage=%d windowProcDepth=%d foreground=%d\r\n",
                (int)InterlockedCompareExchange(&uiStage,0,0),(int)InterlockedCompareExchange(&lastWindowMessage,0,0),
                (int)InterlockedCompareExchange(&insideWindowProc,0,0),GetForegroundWindow()==gameWindow?1:0);
            record(line);
        }
        if(!late && stalled)record("UI heartbeat resumed.\r\n");
        stalled=late;
    }
    return 0;
}
